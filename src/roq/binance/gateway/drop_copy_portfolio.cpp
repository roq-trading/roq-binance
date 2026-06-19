/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/gateway/drop_copy_portfolio.hpp"

#include "roq/mask.hpp"

#include "roq/utils/common.hpp"
#include "roq/utils/update.hpp"

#include "roq/utils/exceptions/unhandled.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/binance/protocol/json/map.hpp"
#include "roq/binance/protocol/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace gateway {

// === CONSTANTS ===

namespace {
auto const NAME = "ex"sv;

auto const SUPPORTS = Mask{
    SupportType::ORDER_ACK,
    SupportType::ORDER,
    SupportType::TRADE,
    SupportType::FUNDS,
};

size_t const MAX_DECODE_BUFFER_DEPTH = 1;
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto &account) {
  return fmt::format("{}:{}:{}"sv, stream_id, NAME, account);
}

auto create_query(auto &listen_key) {
  assert(!std::empty(listen_key));
  return fmt::format("/{}"sv, listen_key);
}

auto create_uri(auto &settings) {
  auto &uri = settings.ws.pm_uri;
  auto result = fmt::format("{}://{}{}"sv, uri.get_scheme(), uri.get_host(), uri.get_path());
  return io::web::URI{result};
}

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = create_uri(settings);
  auto config = web::socket::Client::Config{
      // connection
      .interface = {},
      .uris = {&uri, 1},
      .host = settings.ws.host,
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
      .ping_frequency = settings.ws.ping_freq,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::socket::Client::create(handler, context, config, []() -> std::string { return {}; });
}

struct create_metrics final : public utils::metrics::Factory {
  create_metrics(auto &settings, auto &group, auto const &function) : utils::metrics::Factory{settings.app.name, group, function} {}
};
}  // namespace

// === IMPLEMENTATION ===

DropCopyPortfolio::DropCopyPortfolio(
    DropCopy::Handler &handler,
    io::Context &context,
    uint16_t stream_id,
    Account &account,
    Shared &shared,
    Request &request,
    std::string_view const &listen_key,
    [[maybe_unused]] MarginMode margin_mode)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.name)}, query_{create_query(listen_key)},
      connection_{create_connection(*this, shared.settings, context)}, decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(shared.settings, name_, "parse"sv),
          .outbound_account_position = create_metrics(shared.settings, name_, "outbound_account_position"sv),
          .balance_update = create_metrics(shared.settings, name_, "balance_update"sv),
          .execution_report = create_metrics(shared.settings, name_, "execution_report"sv),
          .list_status = create_metrics(shared.settings, name_, "list_status"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
          .heartbeat = create_metrics(shared.settings, name_, "heartbeat"sv),
      },
      account_{account}, shared_{shared}, request_{request}, download_{{}, [this](auto state) { return download(state); }} {
  log::info<5>(R"(stream_id={}, account="{}", listen_key="{}")"sv, stream_id_, account_.name, listen_key);
  assert(margin_mode == MarginMode::PORTFOLIO);
}

void DropCopyPortfolio::operator()(Event<Start> const &) {
  (*connection_).start();
}

void DropCopyPortfolio::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void DropCopyPortfolio::operator()(Event<Timer> const &event) {
  (*connection_).refresh(event.value.now);
  check_response_account();
  check_response_orders();
  check_response_trades();
}

void DropCopyPortfolio::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.parse, metrics::Type::PROFILE)
      .write(profile_.outbound_account_position, metrics::Type::PROFILE)
      .write(profile_.balance_update, metrics::Type::PROFILE)
      .write(profile_.execution_report, metrics::Type::PROFILE)
      .write(profile_.list_status, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      .write(latency_.heartbeat, metrics::Type::LATENCY);
}

void DropCopyPortfolio::operator()(web::socket::Client::Connected const &) {
}

void DropCopyPortfolio::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
}

void DropCopyPortfolio::operator()(web::socket::Client::Ready const &) {
  download_.begin();
}

void DropCopyPortfolio::operator()(web::socket::Client::Close const &) {
}

void DropCopyPortfolio::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = account_.name,
      .latency = latency.sample,
  };
  create_trace_and_dispatch(shared_.dispatcher, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void DropCopyPortfolio::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void DropCopyPortfolio::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected"sv);
}

void DropCopyPortfolio::operator()(ConnectionStatus connection_status, std::string_view const &reason) {
  connection_status_ = connection_status;
  TraceInfo trace_info;
  auto stream_status = StreamStatus{
      .stream_id = stream_id_,
      .account = account_.name,
      .supports = SUPPORTS,
      .transport = Transport::TCP,
      .protocol = Protocol::WS,
      .encoding = {Encoding::JSON},
      .priority = Priority::PRIMARY,
      .connection_status = connection_status_,
      .reason = reason,
      .interface = (*connection_).get_interface(),
      .authority = (*connection_).get_current_authority(),
      .path = (*connection_).get_current_path(),
      .proxy = (*connection_).get_proxy(),
  };
  log::info("stream_status={}"sv, stream_status);
  create_trace_and_dispatch(shared_.dispatcher, trace_info, stream_status);
}

uint32_t DropCopyPortfolio::download(State state) {
  switch (state) {
    using enum State;
    case UNDEFINED:
      assert(false);
      break;
    case ACCOUNT:
      (*this)(ConnectionStatus::DOWNLOADING, "account"sv);
      request_account();
      return 1;
    case ORDERS:
      (*this)(ConnectionStatus::DOWNLOADING, "orders"sv);
      request_orders();
      return 1;
    case TRADES:
      if (shared_.settings.download.trades_lookback.count() != 0 && !std::empty(shared_.settings.download.symbols)) {
        (*this)(ConnectionStatus::DOWNLOADING, "trades"sv);
        request_trades();
        return 1;
      } else {
        return 0;
      }
    case DONE:
      (*this)(ConnectionStatus::READY);
      assert(!ready_);
      ready_ = true;
      return 0;
  }
  assert(false);
  return 0;
}

void DropCopyPortfolio::parse(std::string_view const &message) {
  profile_.parse([&]() {
    auto log_message = [&]() { log::warn(R"(*** PLEASE REPORT *** message="{}")"sv, message); };
    try {
      TraceInfo trace_info;
      if (!protocol::json::UserStreamParser::dispatch_papi(*this, message, decode_buffer_, trace_info, shared_.allow_unknown_event_types)) {
        log_message();
      }
    } catch (...) {
      log_message();
      utils::exceptions::Unhandled::terminate();
    }
  });
}

void DropCopyPortfolio::operator()(Trace<protocol::json::OutboundAccountPosition> const &event) {
  profile_.outbound_account_position([&]() {
    auto &[trace_info, outbound_account_position] = event;
    log::info<2>("outbound_account_position={}"sv, outbound_account_position);
    for (auto &item : outbound_account_position.data.balances) {
      auto funds_update = FundsUpdate{
          .stream_id = stream_id_,
          .account = account_.name,
          .currency = item.asset,
          .margin_mode = MarginMode::PORTFOLIO,
          .balance = item.free_amount,
          .hold = item.locked_amount,
          .borrowed = NaN,
          .unrealized_pnl = NaN,
          .external_account = {},
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = outbound_account_position.data.time_of_last_account_update,
          .sending_time_utc = outbound_account_position.data.event_time,
      };
      create_trace_and_dispatch(shared_.dispatcher, trace_info, funds_update, true);
    }
  });
}

void DropCopyPortfolio::operator()(Trace<protocol::json::BalanceUpdate> const &event) {
  profile_.balance_update([&]() {
    auto &[trace_info, balance_update] = event;
    log::info<2>("balance_update={}"sv, balance_update);
    // note! contains delta (changes) -- we're not going to use here
  });
}

void DropCopyPortfolio::operator()(Trace<protocol::json::ExecutionReport> const &event) {
  profile_.execution_report([&]() {
    auto &[trace_info, execution_report] = event;
    log::info<2>("execution_report={}"sv, execution_report);
    auto external_order_id = fmt::format("{}"sv, execution_report.data.order_id);  // alloc
    auto average_traded_price = utils::is_zero(execution_report.data.cumulative_filled_quantity)
                                    ? NaN
                                    : (execution_report.data.cumulative_quote_asset_transacted_quantity / execution_report.data.cumulative_filled_quantity);
    auto last_liquidity = execution_report.data.is_trade_maker ? Liquidity::MAKER : Liquidity::TAKER;
    auto order_update = server::oms::OrderUpdate{
        .account = account_.name,
        .exchange = shared_.settings.exchange,
        .symbol = execution_report.data.symbol,
        .side = map(execution_report.data.side),
        .position_effect = {},
        .margin_mode = MarginMode::PORTFOLIO,
        .max_show_quantity = NaN,
        .order_type = map(execution_report.data.order_type),
        .time_in_force = map(execution_report.data.time_in_force),
        .execution_instructions = {},
        .create_time_utc = {},
        .update_time_utc = execution_report.data.transaction_time,
        .external_account = {},
        .external_order_id = external_order_id,
        .client_order_id = execution_report.data.client_order_id,
        .order_status = map(execution_report.data.current_order_status),
        .error = {},
        .text = {},
        .quantity = NaN,
        .price = execution_report.data.price,
        .stop_price = execution_report.data.stop_price,
        .leverage = NaN,
        .remaining_quantity = NaN,
        .traded_quantity = execution_report.data.cumulative_filled_quantity,
        .average_traded_price = average_traded_price,
        .last_traded_quantity = execution_report.data.last_executed_quantity,
        .last_traded_price = execution_report.data.last_executed_price,
        .last_liquidity = last_liquidity,
        .routing_id = {},
        .max_request_version = {},
        .max_response_version = {},
        .max_accepted_version = {},
        .update_type = UpdateType::INCREMENTAL,
        .sending_time_utc = execution_report.data.event_time,
    };
    auto user_id = SOURCE_NONE;
    auto order_id = ORDER_ID_NONE;
    auto strategy_id = STRATEGY_ID_NONE;
    auto callback = [&](auto &order) {
      user_id = order.user_id;
      order_id = order.order_id;
      strategy_id = order.strategy_id;
    };
    create_trace_and_dispatch(shared_.dispatcher, trace_info, order_update, stream_id_, callback);
    if (execution_report.data.current_execution_type != protocol::json::ExecutionType::TRADE) {
      return;
    }
    auto side = map(execution_report.data.side).template get<Side>();
    auto ref_data = shared_.dispatcher.get_ref_data(shared_.settings.exchange, execution_report.data.symbol);
    auto profit_loss_amount =
        utils::compute_profit_loss_amount(side, execution_report.data.last_executed_quantity, execution_report.data.last_executed_price, ref_data.multiplier);
    auto fill = Fill{
        .external_trade_id = {},
        .quantity = execution_report.data.last_executed_quantity,
        .price = execution_report.data.last_executed_price,
        .liquidity = last_liquidity,
        .commission_amount = execution_report.data.commission_amount,
        .commission_currency = execution_report.data.commission_asset,
        .base_amount = NaN,
        .quote_amount = execution_report.data.last_quote_asset_transacted_quantity,
        .profit_loss_amount = profit_loss_amount,
    };
    fmt::format_to(std::back_inserter(fill.external_trade_id), "{}"sv, execution_report.data.trade_id);
    auto trade_update = TradeUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .order_id = order_id,
        .exchange = shared_.settings.exchange,
        .symbol = execution_report.data.symbol,
        .side = side,
        .position_effect = {},
        .margin_mode = MarginMode::PORTFOLIO,
        .quantity_type = {},
        .create_time_utc = execution_report.data.transaction_time,
        .update_time_utc = execution_report.data.transaction_time,
        .external_account = {},
        .external_order_id = external_order_id,
        .client_order_id = execution_report.data.client_order_id,
        .fills = {&fill, 1},
        .routing_id = {},
        .update_type = UpdateType::INCREMENTAL,
        .sending_time_utc = execution_report.data.event_time,
        .user = {},
        .strategy_id = strategy_id,
    };
    create_trace_and_dispatch(shared_.dispatcher, trace_info, trade_update, true, user_id);
  });
}

void DropCopyPortfolio::operator()(Trace<protocol::json::ListStatus> const &event) {
  profile_.list_status([&]() {
    auto &[trace_info, list_status] = event;
    log::info<2>("list_status={}"sv, list_status);
  });
}

void DropCopyPortfolio::request_account() {
  log::info("Requesting account download..."sv);
  request_.request_account = clock::get_system();
}

void DropCopyPortfolio::check_response_account() {
  if (download_.state() != State::ACCOUNT) {
    return;
  }
  if (request_.request_account < request_.respond_account) {
    log::info("Account download has completed!"sv);
    download_.check(State::ACCOUNT);
  }
}

void DropCopyPortfolio::request_orders() {
  log::info("Requesting order download..."sv);
  request_.request_orders = clock::get_system();
}

void DropCopyPortfolio::check_response_orders() {
  if (download_.state() != State::ORDERS) {
    return;
  }
  if (request_.request_orders < request_.respond_orders) {
    log::info("Order download has completed!"sv);
    download_.check(State::ORDERS);
  }
}

void DropCopyPortfolio::request_trades() {
  log::info("Requesting trades download..."sv);
  request_.request_trades = clock::get_system();
}

void DropCopyPortfolio::check_response_trades() {
  if (download_.state() != State::TRADES) {
    return;
  }
  if (request_.request_trades < request_.respond_trades) {
    log::info("Trade download has completed!"sv);
    download_.check(State::TRADES);
  }
}

}  // namespace gateway
}  // namespace binance
}  // namespace roq
