/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/drop_copy_margin.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/common.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/utils/exceptions/unhandled.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/binance/json/encoder.hpp"
#include "roq/binance/json/map.hpp"
#include "roq/binance/json/utils.hpp"
#include "roq/binance/json/wsapi_type.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const NAME = "ws"sv;

auto const SUPPORTS = Mask{
    SupportType::ORDER_ACK,
    SupportType::ORDER,
    SupportType::FUNDS,
};

uint32_t const REQUEST_ID = 1'000'000;

size_t const MAX_DECODE_BUFFER_DEPTH = 1;

size_t const DOWNLOAD_TRADES_LIMIT = 1000;
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto &account) {
  return fmt::format("{}:{}:{}"sv, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &settings, auto &context, auto &account) {
  auto uri = settings.ws_api_2.uri;
  auto config = web::socket::Client::Config{
      // connection
      .interface = {},
      .uris = {&uri, 1},
      .host = settings.ws_api_2.host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = settings.net.connection_timeout,
      .disconnect_on_idle_timeout = {},
      .always_reconnect = true,
      // proxy
      .proxy = {},
      // http
      .user_agent = ROQ_PACKAGE_NAME,
      .request_timeout = {},
      .ping_frequency = settings.ws_api_2.ping_freq,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::socket::Client::create(handler, context, config, [headers = std::string{account.get_rest_headers()}]() { return headers; });
}

struct create_metrics final : public utils::metrics::Factory {
  create_metrics(auto &settings, auto &group, auto const &function) : utils::metrics::Factory{settings.app.name, group, function} {}
  create_metrics(auto &settings, auto &group, auto const &function, auto const &params) : utils::metrics::Factory{settings.app.name, group, function, params} {}
};

auto get_download_trades_lookback(auto const &settings, auto download_trades_is_first) {
  if (download_trades_is_first) {
    if (settings.download.trades_lookback_on_restart.count()) {
      return settings.download.trades_lookback_on_restart;
    }
  }
  return settings.download.trades_lookback;
}
}  // namespace

// === IMPLEMENTATION ===

DropCopyMargin::DropCopyMargin(
    DropCopy::Handler &handler,
    io::Context &context,
    uint16_t stream_id,
    Account &account,
    Shared &shared,
    Request &request,
    std::string_view const &listen_key,
    MarginMode margin_mode)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.name)}, listen_key_{listen_key}, margin_mode_{margin_mode},
      connection_{create_connection(*this, shared.settings, context, account)},
      decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(shared.settings, name_, "parse"sv),
          //
          .session_logon = create_metrics(shared.settings, name_, "session_logon"sv),
          //
          .user_data_stream_subscribe = create_metrics(shared.settings, name_, "user_data_stream_subscribe"sv),
          //
          .outbound_account_position = create_metrics(shared.settings, name_, "outbound_account_position"sv),
          .balance_update = create_metrics(shared.settings, name_, "balance_update"sv),
          .execution_report = create_metrics(shared.settings, name_, "execution_report"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
          .heartbeat = create_metrics(shared.settings, name_, "heartbeat"sv),
      },
      rate_limiter_{
          .request_weight_1m = create_metrics(shared.settings, name_, "request_weight"sv, "1m"sv),
          .create_order_10s = create_metrics(shared.settings, name_, "create_order"sv, "10s"sv),
          .create_order_1d = create_metrics(shared.settings, name_, "create_order"sv, "1d"sv),
      },
      account_{account}, shared_{shared}, request_{request}, request_id_{REQUEST_ID * stream_id},
      download_{shared.settings.rest.request_timeout, [this](auto state) { return download(state); }} {
  log::info<5>(R"(stream_id={}, account="{}")"sv, stream_id_, account_.name);
}

void DropCopyMargin::operator()(Event<Start> const &) {
  (*connection_).start();
}

void DropCopyMargin::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void DropCopyMargin::operator()(Event<Timer> const &event) {
  auto &[message_info, timer] = event;
  (*connection_).refresh(timer.now);
  check_response_account();
  check_response_orders();
}

void DropCopyMargin::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.parse, metrics::Type::PROFILE)
      //
      .write(profile_.session_logon, metrics::Type::PROFILE)
      //
      .write(profile_.user_data_stream_subscribe, metrics::Type::PROFILE)
      //
      .write(profile_.outbound_account_position, metrics::Type::PROFILE)
      .write(profile_.balance_update, metrics::Type::PROFILE)
      .write(profile_.execution_report, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      .write(latency_.heartbeat, metrics::Type::LATENCY)
      // rate limiter
      .write(rate_limiter_.request_weight_1m, metrics::Type::RATE_LIMITER)
      .write(rate_limiter_.create_order_10s, metrics::Type::RATE_LIMITER)
      .write(rate_limiter_.create_order_1d, metrics::Type::RATE_LIMITER);
}

// session-logon

void DropCopyMargin::session_logon() {
  auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
  auto request = json::WSAPIRequest{
      .sequence = ++request_id_,
      .type = json::WSAPIType::SESSION_LOGON,
      .user_id = {},
      .order_id = {},
      .version = {},
      .order_id_2 = {},
  };
  auto request_id = json::WSAPIRequest::encode(request_encode_buffer_, request);
  auto signature = account_.create_session_logon_signature(timestamp);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}",)"
      R"("method":"session.logon",)"
      R"("params":{{)"
      R"("apiKey":"{}",)"
      R"("timestamp":{},)"
      R"("signature":"{}")"
      R"(}})"
      R"(}})"sv,
      request_id,
      account_.get_key(),
      timestamp.count(),
      signature);
  (*connection_).send_text(message);
  (*this)(ConnectionStatus::LOGIN_SENT);
}

// user-data-stream-subscribe

void DropCopyMargin::subscribe_user_data_stream() {
  auto request = json::WSAPIRequest{
      .sequence = ++request_id_,
      .type = json::WSAPIType::USER_DATA_STREAM_SUBSCRIBE,
      .user_id = {},
      .order_id = {},
      .version = {},
      .order_id_2 = {},
  };
  auto request_id = json::WSAPIRequest::encode(request_encode_buffer_, request);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}",)"
      R"("method":"userDataStream.subscribe.listenToken",)"
      R"("params":{{)"
      R"("listenToken":"{}")"
      R"(}})"
      R"(}})"sv,
      request_id,
      listen_key_);
  (*connection_).send_text(message);
  (*this)(ConnectionStatus::DOWNLOADING);
}

void DropCopyMargin::get_account() {
  request_.request_account_cross = clock::get_system();
  (*this)(ConnectionStatus::DOWNLOADING);
}

// open-orders

void DropCopyMargin::get_orders() {
  request_.request_orders_cross = clock::get_system();
  (*this)(ConnectionStatus::DOWNLOADING);
}

void DropCopyMargin::operator()(web::socket::Client::Connected const &) {
  // wait for ready
}

void DropCopyMargin::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
}

void DropCopyMargin::operator()(web::socket::Client::Ready const &) {
  download_.begin();
}

void DropCopyMargin::operator()(web::socket::Client::Close const &) {
}

void DropCopyMargin::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = account_.name,
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void DropCopyMargin::operator()(web::socket::Client::Text const &text) {
  log::warn("DEBUG {}"sv, text.payload);
  parse(text.payload);
}

void DropCopyMargin::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected"sv);
}

void DropCopyMargin::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    auto stream_status = StreamStatus{
        .stream_id = stream_id_,
        .account = account_.name,
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::WS,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
        .interface = (*connection_).get_interface(),
        .authority = (*connection_).get_current_authority(),
        .path = (*connection_).get_current_path(),
        .proxy = (*connection_).get_proxy(),
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t DropCopyMargin::download(DropCopyStateMargin state) {
  switch (state) {
    using enum DropCopyStateMargin;
    case UNDEFINED:
      assert(false);
      break;
    case SESSION_LOGON:
      session_logon();
      return 1;
    case USER_DATA_STREAM_SUBSCRIBE:
      subscribe_user_data_stream();
      return 1;
    case ACCOUNT:
      get_account();
      return 1;
    case ORDERS:
      get_orders();
      return 1;
    case DONE:
      (*this)(ConnectionStatus::READY);
      assert(!ready_);
      ready_ = true;
      return 0;
  }
  assert(false);
  return 0;
}

void DropCopyMargin::parse(std::string_view const &message) {
  profile_.parse([&]() {
    auto log_message = [&]() { log::warn(R"(*** PLEASE REPORT *** message="{}")"sv, message); };
    try {
      TraceInfo trace_info;
      if (!json::WSAPIParser::dispatch(*this, message, decode_buffer_, trace_info)) {
        log_message();
      }
    } catch (...) {
      log_message();
      utils::exceptions::Unhandled::terminate();
    }
  });
}

// json::WSAPIParser::Handler

void DropCopyMargin::operator()(Trace<json::WSAPISessionLogon> const &event) {
  auto const STATE = DropCopyStateMargin::SESSION_LOGON;
  profile_.session_logon([&]() {
    auto &[trace_info, wsapi_session_logon] = event;
    log::info<2>("wsapi_session_logon={}"sv, wsapi_session_logon);
    if (wsapi_session_logon.status == 200) {
      [[maybe_unused]] auto &result = wsapi_session_logon.result;
      download_.check_relaxed(STATE);
    } else {
      // XXX FIXME TODO review
      [[maybe_unused]] auto error = json::guess_error(wsapi_session_logon.error.code);
      log::error(R"(Unexpected: account="{}", error={})"sv, account_.name, wsapi_session_logon.error);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    }
    update_rate_limits(event);
  });
}

void DropCopyMargin::operator()(Trace<json::WSAPIUserDataStreamSubscribe> const &event) {
  auto const STATE = DropCopyStateMargin::USER_DATA_STREAM_SUBSCRIBE;
  profile_.user_data_stream_subscribe([&]() {
    auto &[trace_info, wsapi_user_data_stream_subscribe] = event;
    log::info<2>("wsapi_user_data_stream_subscribe={}"sv, wsapi_user_data_stream_subscribe);
    if (wsapi_user_data_stream_subscribe.status == 200) {
      download_.check_relaxed(STATE);
    } else {
      // XXX FIXME TODO review
      [[maybe_unused]] auto error = json::guess_error(wsapi_user_data_stream_subscribe.error.code);
      log::error(R"(Unexpected: account="{}", error={})"sv, account_.name, wsapi_user_data_stream_subscribe.error);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    }
    update_rate_limits(event);
  });
}

void DropCopyMargin::operator()(Trace<json::WSAPIAccount> const &event) {
  log::fatal("{}"sv, event.value);
  /*
  auto const STATE = DropCopyStateMargin::ACCOUNT_STATUS;
  profile_.account_status([&]() {
    auto &[trace_info, wsapi_account] = event;
    log::info<2>("wsapi_account={}"sv, wsapi_account);
    if (wsapi_account.status == 200) {
      auto &account = wsapi_account.result;
      for (auto &item : account.balances) {
        auto funds_update = FundsUpdate{
            .stream_id = stream_id_,
            .account = account_.name,
            .currency = item.asset,
            .margin_mode = {},
            .balance = item.free,
            .hold = item.locked,
            .borrowed = NaN,
            .external_account = {},
            .update_type = UpdateType::SNAPSHOT,
            .exchange_time_utc = account.update_time,
            .sending_time_utc = account.update_time,
        };
        create_trace_and_dispatch(handler_, trace_info, funds_update, true);
      }
      download_.check_relaxed(STATE);
    } else {
      // XXX FIXME TODO review
      log::error(R"(Unexpected: account="{}", error={})"sv, account_.name, wsapi_account.error);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    }
    update_rate_limits(event);
  });
  */
}

void DropCopyMargin::operator()(Trace<json::WSAPIOpenOrders> const &event) {
  log::fatal("{}"sv, event.value);
  /*
  auto const STATE = DropCopyStateMargin::OPEN_ORDERS_STATUS;
  profile_.open_orders_status([&]() {
    auto &[trace_info, wsapi_open_orders] = event;
    log::info<2>("wsapi_open_orders={}"sv, wsapi_open_orders);
    if (wsapi_open_orders.status == 200) {
      auto &open_orders = wsapi_open_orders.result;
      for (auto &item : open_orders) {
        log::info<2>("item={}"sv, item);
        if (std::empty(item.client_order_id)) {
          continue;
        }
        open_orders_symbols_.emplace(item.symbol);
        auto external_order_id = fmt::format("{}"sv, item.order_id);  // alloc
        auto order_update = server::oms::OrderUpdate{
            .account = account_.name,
            .exchange = shared_.settings.exchange,
            .symbol = item.symbol,
            .side = map(item.side),
            .position_effect = {},
            .margin_mode = {},
            .max_show_quantity = NaN,
            .order_type = map(item.type),
            .time_in_force = map(item.time_in_force),
            .execution_instructions = {},
            .create_time_utc = item.time,
            .update_time_utc = item.update_time,
            .external_account = {},
            .external_order_id = external_order_id,
            .client_order_id = item.client_order_id,
            .order_status = map(item.status),
            .error = {},
            .text = {},
            .quantity = item.orig_qty,
            .price = item.price,
            .stop_price = item.stop_price,
            .leverage = NaN,
            .remaining_quantity = NaN,
            .traded_quantity = item.executed_qty,
            .average_traded_price = {},
            .last_traded_quantity = {},
            .last_traded_price = {},
            .last_liquidity = {},
            .routing_id = {},
            .max_request_version = {},
            .max_response_version = {},
            .max_accepted_version = {},
            .update_type = UpdateType::SNAPSHOT,
            .sending_time_utc = {},
        };
        Trace event_2{trace_info, order_update};
        (*this)(event_2, item.client_order_id);
      }
      download_.check_relaxed(STATE);
    } else {
      // XXX FIXME TODO review
      log::error(R"(Unexpected: account="{}", error={})"sv, account_.name, wsapi_open_orders.error);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    }
    update_rate_limits(event);
  });
  */
}

void DropCopyMargin::operator()(Trace<json::WSAPITrades> const &event) {
  log::fatal("{}"sv, event.value);
  /*
  auto const STATE = DropCopyStateMargin::MY_TRADES;
  profile_.my_trades([&]() {
    auto &[trace_info, wsapi_trades] = event;
    log::info<2>("wsapi_trades={}"sv, wsapi_trades);
    if (wsapi_trades.status == 200) {
      download_trades_is_first_ = false;  // after first successful
      auto &trades = wsapi_trades.result;
      for (auto &item : trades) {
        log::info<2>("item={}"sv, item);
        auto liquidity = item.is_maker ? Liquidity::MAKER : Liquidity::TAKER;
        auto side = item.is_buyer ? Side::BUY : Side::SELL;
        auto ref_data = shared_.get_ref_data(shared_.settings.exchange, item.symbol);
        auto profit_loss_amount = utils::compute_profit_loss_amount(side, item.qty, item.price, ref_data.multiplier);
        auto fill = Fill{
            .external_trade_id = {},
            .quantity = item.qty,  // XXX FIXME quote_qty ???
            .price = item.price,
            .liquidity = liquidity,
            .commission_amount = item.commission,
            .commission_currency = item.commission_asset,
            .base_amount = NaN,
            .quote_amount = item.quote_qty,
            .profit_loss_amount = profit_loss_amount,
        };
        fmt::format_to(std::back_inserter(fill.external_trade_id), "{}"sv, item.id);
        auto external_order_id = fmt::format("{}"sv, item.order_id);
        auto trade_update = TradeUpdate{
            .stream_id = stream_id_,
            .account = account_.name,
            .order_id = {},
            .exchange = shared_.settings.exchange,
            .symbol = item.symbol,
            .side = side,
            .position_effect = {},
            .margin_mode = {},
            .quantity_type = {},
            .create_time_utc = item.time,
            .update_time_utc = item.time,
            .external_account = {},
            .external_order_id = external_order_id,
            .client_order_id = {},
            .fills = {&fill, 1},
            .routing_id = {},
            .update_type = UpdateType::INCREMENTAL,
            .sending_time_utc = item.time,
            .user = {},
            .strategy_id = {},
        };
        std::string_view client_order_id;  // XXX MISSING
        create_trace_and_dispatch(handler_, trace_info, trade_update, true, SOURCE_NONE, client_order_id);
      }
      // XXX FIXME TODO here we need a countdown
      download_.check_relaxed(STATE);
    }
    // XXX FIXME TODO review
    else {
      log::error(R"(Unexpected: account="{}", error={})"sv, account_.name, wsapi_trades.error);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    }
    update_rate_limits(event);
  });
  */
}

void DropCopyMargin::operator()(Trace<json::WSAPIOrderPlace> const &, json::WSAPIRequest const &) {
  log::fatal("Unexpected"sv);
}

void DropCopyMargin::operator()(Trace<json::WSAPIOrderAmendKeepPriority> const &, json::WSAPIRequest const &) {
  log::fatal("Unexpected"sv);
}

void DropCopyMargin::operator()(Trace<json::WSAPICancelOrder> const &, json::WSAPIRequest const &) {
  log::fatal("Unexpected"sv);
}

void DropCopyMargin::operator()(Trace<json::WSAPICancelOpenOrders> const &, json::WSAPIRequest const &) {
  log::fatal("Unexpected"sv);
}

void DropCopyMargin::operator()(Trace<json::WSAPIOutboundAccountPosition> const &event) {
  profile_.outbound_account_position([&]() {
    auto &[trace_info, wsapi_outbound_account_position] = event;
    log::info<2>("wsapi_outbound_account_position={}"sv, wsapi_outbound_account_position);
    log::warn("DEBUG wsapi_outbound_account_position={}"sv, wsapi_outbound_account_position);
    Trace event_2{trace_info, wsapi_outbound_account_position.event};
    (*this)(event_2);
  });
}

void DropCopyMargin::operator()(Trace<json::WSAPIBalanceUpdate> const &event) {
  profile_.balance_update([&]() {
    auto &[trace_info, wsapi_balance_update] = event;
    log::info<2>("wsapi_balance_update={}"sv, wsapi_balance_update);
    log::warn("DEBUG wsapi_balance_update={}"sv, wsapi_balance_update);
    Trace event_2{trace_info, wsapi_balance_update.event};
    (*this)(event_2);
  });
}

void DropCopyMargin::operator()(Trace<json::WSAPIExecutionReport> const &event) {
  profile_.execution_report([&]() {
    auto &[trace_info, wsapi_execution_report] = event;
    log::info<2>("wsapi_execution_report={}"sv, wsapi_execution_report);
    log::warn("DEBUG wsapi_execution_report={}"sv, wsapi_execution_report);
    Trace event_2{trace_info, wsapi_execution_report.event};
    (*this)(event_2);
  });
}

// helpers

void DropCopyMargin::operator()(Trace<json::OutboundAccountPositionData> const &event) {
  auto &[trace_info, outbound_account_position] = event;
  for (auto &item : outbound_account_position.balances) {
    auto funds_update = FundsUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .currency = item.asset,
        .margin_mode = {},  // XXX FIXME TODO margin_mode
        .balance = item.free_amount,
        .hold = item.locked_amount,
        .borrowed = NaN,
        .external_account = {},
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = outbound_account_position.time_of_last_account_update,
        .sending_time_utc = outbound_account_position.event_time,
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

void DropCopyMargin::operator()(Trace<json::BalanceUpdateData> const &) {
  // note! contains delta (changes) -- we're not going to use here
}

void DropCopyMargin::operator()(Trace<json::ExecutionReportData> const &event) {
  auto &[trace_info, execution_report] = event;
  auto external_order_id = fmt::format("{}"sv, execution_report.order_id);  // alloc
  auto average_traded_price = utils::is_zero(execution_report.cumulative_filled_quantity)
                                  ? NaN
                                  : (execution_report.cumulative_quote_asset_transacted_quantity / execution_report.cumulative_filled_quantity);
  auto last_liquidity = execution_report.is_trade_maker ? Liquidity::MAKER : Liquidity::TAKER;
  auto order_update = server::oms::OrderUpdate{
      .account = account_.name,
      .exchange = shared_.settings.exchange,
      .symbol = execution_report.symbol,
      .side = map(execution_report.side),
      .position_effect = {},
      .margin_mode = {},  // XXX FIXME TODO margin_mode
      .max_show_quantity = NaN,
      .order_type = map(execution_report.order_type),
      .time_in_force = map(execution_report.time_in_force),
      .execution_instructions = {},
      .create_time_utc = {},
      .update_time_utc = execution_report.transaction_time,
      .external_account = {},
      .external_order_id = external_order_id,
      .client_order_id = {},
      .order_status = map(execution_report.current_order_status),
      .error = {},
      .text = {},
      .quantity = NaN,
      .price = execution_report.price,
      .stop_price = execution_report.stop_price,
      .leverage = NaN,
      .remaining_quantity = NaN,
      .traded_quantity = execution_report.cumulative_filled_quantity,
      .average_traded_price = average_traded_price,
      .last_traded_quantity = execution_report.last_executed_quantity,
      .last_traded_price = execution_report.last_executed_price,
      .last_liquidity = last_liquidity,
      .routing_id = {},
      .max_request_version = {},
      .max_response_version = {},
      .max_accepted_version = {},
      .update_type = UpdateType::INCREMENTAL,
      .sending_time_utc = execution_report.event_time,
  };
  auto user_id = SOURCE_NONE;
  auto order_id = ORDER_ID_NONE;
  auto strategy_id = STRATEGY_ID_NONE;
  if (shared_.update_order(execution_report.client_order_id, stream_id_, trace_info, order_update, [&](auto &order) {
        user_id = order.user_id;
        order_id = order.order_id;
        strategy_id = order.strategy_id;
      })) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
    log::warn("execution_report={}"sv, execution_report);
  }
  if (execution_report.current_execution_type != json::ExecutionType::TRADE) {
    return;
  }
  auto side = map(execution_report.side).template get<Side>();
  auto ref_data = shared_.get_ref_data(shared_.settings.exchange, execution_report.symbol);
  auto profit_loss_amount =
      utils::compute_profit_loss_amount(side, execution_report.last_executed_quantity, execution_report.last_executed_price, ref_data.multiplier);
  auto fill = Fill{
      .external_trade_id = {},
      .quantity = execution_report.last_executed_quantity,
      .price = execution_report.last_executed_price,
      .liquidity = last_liquidity,
      .commission_amount = execution_report.commission_amount,
      .commission_currency = execution_report.commission_asset,
      .base_amount = NaN,
      .quote_amount = execution_report.last_quote_asset_transacted_quantity,
      .profit_loss_amount = profit_loss_amount,
  };
  fmt::format_to(std::back_inserter(fill.external_trade_id), "{}"sv, execution_report.trade_id);
  auto trade_update = TradeUpdate{
      .stream_id = stream_id_,
      .account = account_.name,
      .order_id = order_id,
      .exchange = shared_.settings.exchange,
      .symbol = execution_report.symbol,
      .side = side,
      .position_effect = {},
      .margin_mode = {},  // XXX FIXME TODO margin_mode
      .quantity_type = {},
      .create_time_utc = execution_report.transaction_time,
      .update_time_utc = execution_report.transaction_time,
      .external_account = {},
      .external_order_id = external_order_id,
      .client_order_id = {},
      .fills = {&fill, 1},
      .routing_id = {},
      .update_type = UpdateType::INCREMENTAL,
      .sending_time_utc = execution_report.event_time,
      .user = {},
      .strategy_id = strategy_id,
  };
  create_trace_and_dispatch(handler_, trace_info, trade_update, true, user_id, execution_report.client_order_id);
}

void DropCopyMargin::update_rate_limits(auto &event) {
  auto &[trace_info, message] = event;
  shared_.rate_limits.clear();
  for (auto &item : message.rate_limits) {
    auto type = [&]() -> RateLimitType {
      switch (item.rate_limit_type) {
        using enum json::RateLimitType::type_t;
        case UNDEFINED_INTERNAL:
        case UNKNOWN_INTERNAL:
          break;
        case ORDERS:
          return RateLimitType::CREATE_ORDER;
        case REQUEST_WEIGHT:
          return RateLimitType::REQUEST_WEIGHT;
        case RAW_REQUESTS:
          return RateLimitType::REQUEST;
      }
      return {};
    }();
    if (type == RateLimitType{}) {
      continue;
    }
    auto period = [&]() -> std::chrono::seconds {
      switch (item.interval) {
        using enum json::Interval::type_t;
        case UNDEFINED_INTERNAL:
        case UNKNOWN_INTERNAL:
          break;
        case SECOND:
          return item.interval_num * 1s;
        case MINUTE:
          return item.interval_num * 1min;
        case DAY:
          return item.interval_num * 24h;
      };
      return {};
    }();
    switch (type) {
      using enum RateLimitType;
      case UNDEFINED:
        break;
      case ORDER_ACTION:
        break;
      case CREATE_ORDER:
        if (period == 10s) {
          rate_limiter_.create_order_10s.set(item.count);
        } else if (period == 24h) {
          rate_limiter_.create_order_1d.set(item.count);
        }
        break;
      case REQUEST:
        break;
      case REQUEST_WEIGHT:
        if (period == 1min) {
          rate_limiter_.request_weight_1m.set(item.count);
        }
        break;
    }
    auto rate_limit = RateLimit{
        .type = type,
        .period = period,
        .end_time_utc = {},
        .limit = item.limit,
        .value = item.count,
    };
    shared_.rate_limits.emplace_back(rate_limit);
  }
  if (!std::empty(shared_.rate_limits)) {
    auto rate_limits_update = RateLimitsUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .origin = Origin::EXCHANGE,
        .rate_limits = shared_.rate_limits,
    };
    // Trace event_2{trace_info, rate_limits_update};
    // handler_(event_2);
  }
  shared_.rate_limits.clear();
}

template <typename... Args>
void DropCopyMargin::operator()(Trace<server::oms::Response> const &event, uint8_t user_id, uint64_t order_id, Args &&...args) {
  auto &[trace_info, response] = event;
  if (shared_.update_order(user_id, order_id, stream_id_, trace_info, response, std::forward<Args>(args)..., []([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
  }
}

void DropCopyMargin::operator()(Trace<server::oms::OrderUpdate> const &event, std::string_view const &client_order_id) {
  auto &[trace_info, order_update] = event;
  if (shared_.update_order(client_order_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

void DropCopyMargin::check_response_account() {
  if (download_.state() != DropCopyStateMargin::ACCOUNT) {
    return;
  }
  if (request_.request_account_cross < request_.respond_account_cross) {
    log::info("Account download has completed!"sv);
    download_.check(DropCopyStateMargin::ACCOUNT);
  }
}

void DropCopyMargin::check_response_orders() {
  if (download_.state() != DropCopyStateMargin::ORDERS) {
    return;
  }
  if (request_.request_orders_cross < request_.respond_orders_cross) {
    log::info("Orders download has completed!"sv);
    download_.check(DropCopyStateMargin::ORDERS);
  }
}

}  // namespace binance
}  // namespace roq
