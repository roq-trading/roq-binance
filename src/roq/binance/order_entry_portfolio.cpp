/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/order_entry_portfolio.hpp"

#include <tuple>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/common.hpp"
#include "roq/utils/compare.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/utils/charconv/from_chars.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/binance/json/encoder.hpp"
#include "roq/binance/json/error.hpp"
#include "roq/binance/json/map.hpp"
#include "roq/binance/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const NAME = "om"sv;

auto const SUPPORTS = Mask{
    SupportType::CREATE_ORDER,
    SupportType::CANCEL_ORDER,
    SupportType::ORDER_ACK,
    SupportType::TRADE,
    SupportType::FUNDS,
};

auto const X_MBX_USED_WEIGHT_1M = "x-mbx-used-weight-1m"sv;

size_t const MAX_DECODE_BUFFER_DEPTH = 1;

size_t const DOWNLOAD_TRADES_LIMIT = 1000;
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto const &account) {
  return fmt::format("{}:{}:{}"sv, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &settings, auto &shared, auto &context, auto &interface) {
  auto uri = settings.rest.pm_uri;
  auto ping_path = shared.api.papi.ping_path;
  auto config = web::rest::Client::Config{
      // connection
      .interface = interface,
      .proxy = settings.rest.proxy,
      .uris = {&uri, 1},
      .host = settings.rest.pm_host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = {},
      .disconnect_on_idle_timeout = {},
      .connection = web::http::Connection::KEEP_ALIVE,
      // request
      .allow_pipelining = true,
      .request_timeout = settings.rest.request_timeout,
      // response
      .suspend_on_retry_after = true,
      // http
      .query = {},
      .user_agent = ROQ_PACKAGE_NAME,
      .ping_frequency = settings.rest.ping_freq,
      .ping_path = ping_path,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::rest::Client::create(handler, context, config);
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

auto get_retry_after(auto &response) {
  std::chrono::nanoseconds result = {};
  response.dispatch(web::http::Header::RETRY_AFTER, [&](auto &value) {
    try {
      // XXX FIXME could also be a datetime (see https://datatracker.ietf.org/doc/html/rfc7231)
      auto seconds = utils::charconv::from_string_relaxed<int64_t>(value);
      result = std::chrono::seconds{seconds};
    } catch (RuntimeError &) {
      log::warn<5>(R"(Failed to parse text="{}")"sv, value);
    }
  });
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

OrderEntryPortfolio::OrderEntryPortfolio(
    OrderEntry::Handler &handler,
    io::Context &context,
    uint16_t stream_id,
    Account &account,
    Shared &shared,
    Request &request,
    bool master,
    std::string_view const &interface)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.name)}, master_{master},
      connection_{create_connection(*this, shared.settings, shared, context, interface)},
      decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .listen_key = create_metrics(shared.settings, name_, "listen_key"sv),
          .listen_key_ack = create_metrics(shared.settings, name_, "listen_key_ack"sv),
          .account = create_metrics(shared.settings, name_, "account"sv),
          .account_ack = create_metrics(shared.settings, name_, "account_ack"sv),
          .open_orders = create_metrics(shared.settings, name_, "open_orders"sv),
          .open_orders_ack = create_metrics(shared.settings, name_, "open_orders_ack"sv),
          .trades = create_metrics(shared.settings, name_, "trades"sv),
          .trades_ack = create_metrics(shared.settings, name_, "trades_ack"sv),
          .new_order = create_metrics(shared.settings, name_, "new_order"sv),
          .new_order_ack = create_metrics(shared.settings, name_, "new_order_ack"sv),
          .cancel_replace_order = create_metrics(shared.settings, name_, "cancel_replace_order"sv),
          .cancel_replace_order_ack = create_metrics(shared.settings, name_, "cancel_replace_order_ack"sv),
          .cancel_order = create_metrics(shared.settings, name_, "cancel_order"sv),
          .cancel_order_ack = create_metrics(shared.settings, name_, "cancel_order_ack"sv),
          .cancel_all_open_orders = create_metrics(shared.settings, name_, "cancel_all_open_orders"sv),
          .cancel_all_open_orders_ack = create_metrics(shared.settings, name_, "cancel_all_open_orders_ack"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
      },
      rate_limiter_{
          .requests_1m = create_metrics(shared.settings, name_, "requests"sv, "1m"sv),
      },
      account_{account}, shared_{shared}, request_{request}, download_{shared.settings.rest.request_timeout, [this](auto state) { return download(state); }} {
  log::info<5>(R"(stream_id={}, account="{}")"sv, stream_id_, account_.name);
}

void OrderEntryPortfolio::operator()(Event<Start> const &) {
  (*connection_).start();
}

void OrderEntryPortfolio::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void OrderEntryPortfolio::operator()(Event<Timer> const &event) {
  auto &[message_info, timer] = event;
  (*connection_).refresh(timer.now);
  refresh_listen_key(timer.now);
  if (master_ && ready() && !downloading()) {
    if (!downloading() && request_.respond_account < request_.request_account) {
      log::info("Download account..."sv);
      get_account();
      download_account_ = true;
    }
    if (!downloading() && request_.respond_orders < request_.request_orders) {
      log::info("Download orders..."sv);
      get_open_orders();
      download_orders_ = true;
    }
    if (!downloading() && request_.respond_trades < request_.request_trades) {
      log::info("Download trades..."sv);
      get_trades();
      download_trades_ = true;
    }
  }
}

void OrderEntryPortfolio::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.listen_key, metrics::Type::PROFILE)
      .write(profile_.listen_key_ack, metrics::Type::PROFILE)
      .write(profile_.account, metrics::Type::PROFILE)
      .write(profile_.account_ack, metrics::Type::PROFILE)
      .write(profile_.open_orders, metrics::Type::PROFILE)
      .write(profile_.open_orders_ack, metrics::Type::PROFILE)
      .write(profile_.trades, metrics::Type::PROFILE)
      .write(profile_.trades_ack, metrics::Type::PROFILE)
      .write(profile_.new_order, metrics::Type::PROFILE)
      .write(profile_.new_order_ack, metrics::Type::PROFILE)
      .write(profile_.cancel_replace_order, metrics::Type::PROFILE)
      .write(profile_.cancel_replace_order_ack, metrics::Type::PROFILE)
      .write(profile_.cancel_order, metrics::Type::PROFILE)
      .write(profile_.cancel_order_ack, metrics::Type::PROFILE)
      .write(profile_.cancel_all_open_orders, metrics::Type::PROFILE)
      .write(profile_.cancel_all_open_orders_ack, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      // rate limiter
      .write(rate_limiter_.requests_1m, metrics::Type::RATE_LIMITER);
}

uint16_t OrderEntryPortfolio::operator()(
    Event<CreateOrder> const &event, server::oms::Order const &order, server::oms::RefData const &ref_data, std::string_view const &request_id) {
  new_order(event, order, ref_data, request_id);
  return stream_id_;
}

uint16_t OrderEntryPortfolio::operator()(
    Event<ModifyOrder> const &,
    server::oms::Order const &,
    server::oms::RefData const &,
    [[maybe_unused]] std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id) {
  throw server::oms::NotSupported{"not supported"sv};
  return stream_id_;
}

uint16_t OrderEntryPortfolio::operator()(
    Event<CancelOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  cancel_order(event, order, ref_data, request_id, previous_request_id);
  return stream_id_;
}

uint16_t OrderEntryPortfolio::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  cancel_all_open_orders(event, request_id);
  return stream_id_;
}

void OrderEntryPortfolio::operator()(Trace<web::rest::Client::Connected> const &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    download_.begin();
  }
}

void OrderEntryPortfolio::operator()(Trace<web::rest::Client::Disconnected> const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading()) {
    download_.reset();
  }
  download_account_ = false;
  download_orders_ = false;
  download_trades_ = false;
}

void OrderEntryPortfolio::operator()(Trace<web::rest::Client::Header> const &event) {
  auto &[trace_info, header] = event;
  if (utils::case_insensitive_compare(header.name, X_MBX_USED_WEIGHT_1M) == 0) {
    try {
      auto value = utils::charconv::from_string_relaxed<int64_t>(header.value);
      rate_limiter_.requests_1m.set(value);
    } catch (RuntimeError &) {
      log::warn<5>(R"(Failed to parse text="{}")"sv, header.value);
    }
  }
}

void OrderEntryPortfolio::operator()(Trace<web::rest::Client::Latency> const &event) {
  auto &[trace_info, latency] = event;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = account_.name,
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntryPortfolio::operator()(ConnectionStatus connection_status, std::string_view const &reason) {
  connection_status_ = connection_status;
  TraceInfo trace_info;
  auto stream_status = StreamStatus{
      .stream_id = stream_id_,
      .account = account_.name,
      .supports = SUPPORTS,
      .transport = Transport::TCP,
      .protocol = Protocol::HTTP,
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
  create_trace_and_dispatch(handler_, trace_info, stream_status);
}

uint32_t OrderEntryPortfolio::download(OrderEntryState state) {
  switch (state) {
    using enum OrderEntryState;
    case UNDEFINED:
      assert(false);
      break;
    case LISTEN_KEY:
      if (master_) {
        (*this)(ConnectionStatus::DOWNLOADING, "listen-key"sv);
        get_listen_key();
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

// listen-key

void OrderEntryPortfolio::get_listen_key() {
  profile_.listen_key([&]() {
    auto headers = account_.get_rest_headers_old();
    auto request = web::rest::Request{
        .method = web::http::Method::POST,
        .path = shared_.api.papi.listen_key,
        .query = {},
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_listen_key_ack(event);
    };
    (*connection_)("listen-key"sv, request, callback);
  });
}

void OrderEntryPortfolio::get_listen_key_ack(Trace<web::rest::Response> const &event) {
  auto const STATE = OrderEntryState::LISTEN_KEY;
  profile_.listen_key_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(account="{}", origin={}, error={}, status={}, text="{}")"sv, account_.name, origin, error, status, text);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    };
    auto handle_success = [&](auto &body) {
      json::ListenKeyAck listen_key_ack{body};
      Trace event_2{event, listen_key_ack};
      (*this)(event_2);
      download_.check_relaxed(STATE);
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryPortfolio::operator()(Trace<json::ListenKeyAck> const &event) {
  auto &[trace_info, listen_key_ack] = event;
  log::info<2>("listen_key_ack={}"sv, listen_key_ack);
  bool initial = std::empty(listen_key_);
  if (utils::update(listen_key_, listen_key_ack.listen_key)) {
    if (initial) {
      log::info<1>(R"(Listen key has been acquired (value="{}"))"sv, listen_key_);
      auto listen_key_update = ListenKeyUpdate{
          .account = account_.name,
          .margin_mode = MarginMode::PORTFOLIO,
          .listen_key = listen_key_ack.listen_key,
      };
      create_trace_and_dispatch(handler_, trace_info, listen_key_update);
    } else {
      log::info<1>("Listen key has been refreshed!"sv);
    }
  }
  auto now = clock::get_system();
  listen_key_refresh_ = now + shared_.settings.rest.listen_key_refresh;
}

// account

void OrderEntryPortfolio::get_account() {
  profile_.account([&]() {
    auto now_utc = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_rest_signature_old(now_utc);
    auto headers = account_.get_rest_headers_old();
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = shared_.api.papi.balance,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_account_ack(event);
    };
    (*connection_)("balance"sv, request, callback);
  });
}

void OrderEntryPortfolio::get_account_ack(Trace<web::rest::Response> const &event) {
  profile_.account_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(account="{}", origin={}, error={}, status={}, text="{}")"sv, account_.name, origin, error, status, text);
      // completion
      request_.respond_account = clock::get_system();
      download_account_ = false;
    };
    auto handle_success = [&](auto &body) {
      json::BalancesAck balances_ack{body, decode_buffer_};
      Trace event_2{event, balances_ack};
      (*this)(event_2);
      // completion
      request_.respond_account = clock::get_system();
      download_account_ = false;
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryPortfolio::operator()(Trace<json::BalancesAck> const &event) {
  auto &[trace_info, balances_ack] = event;
  for (auto &item : balances_ack.data) {
    log::info<2>("item={}"sv, item);
    auto funds_update = FundsUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .currency = item.asset,
        .margin_mode = MarginMode::PORTFOLIO,
        .balance = item.total_wallet_balance,
        .hold = NaN,
        .borrowed = NaN,
        .unrealized_pnl = NaN,
        .external_account = {},
        .update_type = UpdateType::SNAPSHOT,
        .exchange_time_utc = item.update_time,
        .sending_time_utc = {},
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

// orders

// XXX should prefer to add filter on symbol -- cost is multiplied by number of symbols traded on exchange
void OrderEntryPortfolio::get_open_orders() {
  profile_.open_orders([&]() {
    auto now_utc = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_rest_signature_old(now_utc);
    auto headers = account_.get_rest_headers_old();
    auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
    encode_buffer_.clear();
    fmt::format_to(
        std::back_inserter(encode_buffer_),
        R"({{)"
        R"("timestamp":{})"
        R"(}})"sv,
        timestamp.count());
    std::string body{std::data(encode_buffer_), std::size(encode_buffer_)};  // XXX not used ???
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = shared_.api.papi.margin_open_orders,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_open_orders_ack(event);
    };
    (*connection_)("margin-open-orders"sv, request, callback);
  });
}

void OrderEntryPortfolio::get_open_orders_ack(Trace<web::rest::Response> const &event) {
  profile_.open_orders_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(account="{}", origin={}, error={}, status={}, text="{}")"sv, account_.name, origin, error, status, text);
      // completion
      request_.respond_orders = clock::get_system();
      download_orders_ = false;
    };
    auto handle_success = [&](auto &body) {
      json::OpenOrdersAck open_orders_ack{body, decode_buffer_};
      Trace event_2{event, open_orders_ack};
      (*this)(event_2);
      // completion
      request_.respond_orders = clock::get_system();
      download_orders_ = false;
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryPortfolio::operator()(Trace<json::OpenOrdersAck> const &event) {
  auto &[trace_info, open_orders_ack] = event;
  for (auto &item : open_orders_ack.data) {
    log::info<2>("item={}"sv, item);
    if (std::empty(item.client_order_id)) {
      continue;
    }
    open_orders_symbols_.emplace(item.symbol);
    auto external_order_id = fmt::format("{}"sv, item.order_id);  // alloc
    auto stop_price = utils::compare(item.stop_price, 0.0) == 0 ? NaN : item.stop_price;
    auto remaining_quantity = item.orig_qty - item.executed_qty;
    auto order_update = server::oms::OrderUpdate{
        .account = account_.name,
        .exchange = shared_.settings.exchange,
        .symbol = item.symbol,
        .side = map(item.side),
        .position_effect = {},
        .margin_mode = MarginMode::PORTFOLIO,
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
        .stop_price = stop_price,
        .leverage = NaN,
        .remaining_quantity = remaining_quantity,
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
}

// trades

// note! GET only supports query string (no body)
void OrderEntryPortfolio::get_trades() {
  profile_.trades([&]() {
    auto &symbols = shared_.settings.download.symbols;
    for (auto &symbol : symbols) {
      auto now_utc = clock::get_realtime<std::chrono::milliseconds>();
      auto lookback = get_download_trades_lookback(shared_.settings, download_trades_is_first_);
      auto limit = shared_.settings.download.trades_limit ? shared_.settings.download.trades_limit : DOWNLOAD_TRADES_LIMIT;
      log::info<1>("Download trades: lookback={}"sv, lookback);
      auto headers = account_.get_rest_headers_old();
      auto body = json::Encoder::my_trades_url(encode_buffer_, symbol, lookback, limit, now_utc);
      auto query = account_.create_rest_signature_old_query(now_utc, body);
      auto request = web::rest::Request{
          .method = web::http::Method::GET,
          .path = shared_.api.papi.margin_my_trades,
          .query = query,
          .accept = web::http::Accept::APPLICATION_JSON,
          .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
          .headers = headers,
          .body = {},
          .quality_of_service = {},
      };
      auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
        TraceInfo trace_info;
        Trace event{trace_info, response};
        get_trades_ack(event);
      };
      (*connection_)("margin-my-trades"sv, request, callback);
    }
  });
}

void OrderEntryPortfolio::get_trades_ack(Trace<web::rest::Response> const &event) {
  profile_.trades_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(account="{}", origin={}, error={}, status={}, text="{}")"sv, account_.name, origin, error, status, text);
      // completion
      request_.respond_trades = clock::get_system();
      download_trades_ = false;
    };
    auto handle_success = [&](auto &body) {
      json::TradesAck trades_ack{body, decode_buffer_};
      Trace event_2{event, trades_ack};
      (*this)(event_2);
      // completion
      request_.respond_trades = clock::get_system();
      download_trades_ = false;
      download_trades_is_first_ = false;
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryPortfolio::operator()(Trace<json::TradesAck> const &event) {
  auto &[trace_info, trades_ack] = event;
  for (auto &item : trades_ack.data) {
    log::info<2>("item={}"sv, item);
    auto liquidity = item.is_maker ? Liquidity::MAKER : Liquidity::TAKER;
    auto side = item.is_buyer ? Side::BUY : Side::SELL;
    auto ref_data = shared_.get_ref_data(shared_.settings.exchange, item.symbol);
    auto profit_loss_amount = utils::compute_profit_loss_amount(side, item.qty, item.price, ref_data.multiplier);
    auto fill = Fill{
        .exchange_time_utc = item.time,
        .external_trade_id = {},
        .quantity = item.qty,
        .price = item.price,
        .liquidity = liquidity,
        .commission_amount = item.commission,
        .commission_currency = item.commission_asset,
        .base_amount = NaN,
        .quote_amount = item.quote_qty,
        .profit_loss_amount = profit_loss_amount,
    };
    fmt::format_to(std::back_inserter(fill.external_trade_id), "{}"sv, item.id);
    auto external_order_id = fmt::format("{}"sv, item.order_id);  // alloc
    auto trade_update = TradeUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .order_id = {},
        .exchange = shared_.settings.exchange,
        .symbol = item.symbol,
        .side = side,
        .position_effect = {},
        .margin_mode = MarginMode::PORTFOLIO,
        .quantity_type = {},
        .create_time_utc = item.time,
        .update_time_utc = item.time,
        .external_account = {},
        .external_order_id = external_order_id,
        .client_order_id = {},
        .fills = {&fill, 1},
        .routing_id = {},
        .update_type = UpdateType::SNAPSHOT,
        .sending_time_utc = {},
        .user = {},
        .strategy_id = {},
    };
    std::string_view client_order_id;  // note! unavailable
    create_trace_and_dispatch(handler_, trace_info, trade_update, true, SOURCE_NONE, client_order_id);
  }
}

// ...

void OrderEntryPortfolio::refresh_listen_key(std::chrono::nanoseconds now) {
  if (!ready_) {
    return;
  }
  if (listen_key_refresh_.count() == 0 || now < listen_key_refresh_) {
    return;
  }
  log::info<1>("Refreshing listen key..."sv);
  listen_key_refresh_ = now + shared_.settings.rest.listen_key_refresh;
  get_listen_key();
}

// new-order

void OrderEntryPortfolio::new_order(
    Event<CreateOrder> const &event, server::oms::Order const &order, server::oms::RefData const &ref_data, std::string_view const &request_id) {
  profile_.new_order([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, create_order] = event;
    open_orders_symbols_.emplace(create_order.symbol);
    auto &create_order_template = shared_.get_create_order_template(create_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto now_utc = clock::get_realtime<std::chrono::milliseconds>();
    auto body = json::Encoder::new_order_url(
        encode_buffer_, create_order, order, ref_data, request_id, create_order_template, recv_window, now_utc, shared_.api.margin_side_effect_type);
    auto query = account_.create_rest_signature_old_body(now_utc, body);
    auto headers = account_.get_rest_headers_old();
    auto request = web::rest::Request{
        .method = web::http::Method::POST,
        .path = shared_.api.papi.margin_order,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
        .headers = headers,
        .body = body,
        .quality_of_service = io::QualityOfService::IMMEDIATE,
    };
    log::debug("request={}"sv, request);
    auto callback = [this, user_id = message_info.source, order_id = create_order.order_id]([[maybe_unused]] auto &request_id, auto &response) {
      uint32_t version = 1;
      TraceInfo trace_info;
      Trace event{trace_info, response};
      new_order_ack(event, user_id, order_id, version);
    };
    (*connection_)(request_id, request, callback);
  });
}

void OrderEntryPortfolio::new_order_ack(Trace<web::rest::Response> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  profile_.new_order_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      auto response = server::oms::Response{
          .request_type = RequestType::CREATE_ORDER,
          .origin = origin,
          .request_status = status,
          .error = error,
          .text = text,
          .version = version,
          .request_id = {},
          .external_order_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, user_id, order_id);
    };
    auto handle_success = [&](auto &body) {
      json::NewOrderAck new_order_ack{body, decode_buffer_};
      Trace event_2{event, new_order_ack};
      (*this)(event_2, user_id, order_id, version);
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryPortfolio::operator()(Trace<json::NewOrderAck> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  auto &[trace_info, new_order_ack] = event;
  log::info<2>("new_order_ack={}, user_id={}, order_id={}, version={}"sv, new_order_ack, user_id, order_id, version);
  auto external_order_id = fmt::format("{}"sv, new_order_ack.order_id);  // alloc
  auto order_status = map(new_order_ack.status).template get<OrderStatus>();
  // LIMIT_MAKER orders do not return any order state + we only end up here if we receive HTTP status OK
  if (order_status == OrderStatus{}) {
    order_status = OrderStatus::WORKING;
  }
  auto remaining_quantity = new_order_ack.orig_qty - new_order_ack.executed_qty;
  auto average_traded_price = utils::is_zero(new_order_ack.executed_qty) ? NaN : (new_order_ack.cummulative_quote_qty / new_order_ack.executed_qty);
  auto last_traded_quantity = 0.0;  // note! could also use new_order_ack.executed_qty
  auto tmp = 0.0;
  for (auto &item : new_order_ack.fills) {
    last_traded_quantity += item.qty;
    tmp += item.price * item.qty;
  }
  auto last_traded_price = NaN;  // note! could also use average_traded_price
  if (utils::is_greater(last_traded_quantity, 0.0)) {
    last_traded_price = tmp / last_traded_quantity;
  }
  auto response = server::oms::Response{
      .request_type = RequestType::CREATE_ORDER,
      .origin = Origin::EXCHANGE,
      .request_status = RequestStatus::ACCEPTED,
      .error = {},
      .text = {},
      .version = version,
      .request_id = {},
      .external_order_id = {},
      .quantity = NaN,
      .price = NaN,
  };
  auto order_update = server::oms::OrderUpdate{
      .account = account_.name,
      .exchange = shared_.settings.exchange,
      .symbol = new_order_ack.symbol,
      .side = map(new_order_ack.side),
      .position_effect = {},
      .margin_mode = MarginMode::PORTFOLIO,
      .max_show_quantity = NaN,
      .order_type = map(new_order_ack.type),
      .time_in_force = map(new_order_ack.time_in_force),
      .execution_instructions = {},
      .create_time_utc = {},
      .update_time_utc = new_order_ack.transact_time,
      .external_account = {},
      .external_order_id = external_order_id,
      .client_order_id = {},
      .order_status = order_status,
      .error = {},
      .text = {},
      .quantity = new_order_ack.orig_qty,
      .price = new_order_ack.price,
      .stop_price = NaN,
      .leverage = NaN,
      .remaining_quantity = remaining_quantity,
      .traded_quantity = new_order_ack.executed_qty,
      .average_traded_price = average_traded_price,
      .last_traded_quantity = last_traded_quantity,
      .last_traded_price = last_traded_price,
      .last_liquidity = {},
      .routing_id = {},
      .max_request_version = {},
      .max_response_version = {},
      .max_accepted_version = {},
      .update_type = UpdateType::INCREMENTAL,
      .sending_time_utc = {},
  };
  Trace event_2{trace_info, response};
  (*this)(event_2, user_id, order_id, order_update);
}

// cancel-order

void OrderEntryPortfolio::cancel_order(
    Event<CancelOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  profile_.cancel_order([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, cancel_order] = event;
    auto &cancel_order_template = shared_.get_cancel_order_template(cancel_order.request_template);
    auto now_utc = clock::get_realtime<std::chrono::milliseconds>();
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto body = json::Encoder::cancel_order_url(
        encode_buffer_, cancel_order, order, ref_data, request_id, previous_request_id, cancel_order_template, recv_window, now_utc);
    auto query = account_.create_rest_signature_old_body(now_utc, body);
    auto headers = account_.get_rest_headers_old();
    auto request = web::rest::Request{
        .method = web::http::Method::DELETE,
        .path = shared_.api.papi.margin_order,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
        .headers = headers,
        .body = body,
        .quality_of_service = io::QualityOfService::IMMEDIATE,
    };
    log::debug("request={}"sv, request);
    auto callback = [this, user_id = message_info.source, order_id = cancel_order.order_id, version = cancel_order.version](
                        [[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      cancel_order_ack(event, user_id, order_id, version);
    };
    (*connection_)(request_id, request, callback);
  });
}

void OrderEntryPortfolio::cancel_order_ack(Trace<web::rest::Response> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  profile_.cancel_order_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = origin,
          .request_status = status,
          .error = error,
          .text = text,
          .version = version,
          .request_id = {},
          .external_order_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, user_id, order_id);
    };
    auto handle_success = [&](auto &body) {
      json::CancelOrderAck cancel_order_ack{body};
      Trace event_2{event, cancel_order_ack};
      (*this)(event_2, user_id, order_id, version);
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryPortfolio::operator()(Trace<json::CancelOrderAck> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  auto &[trace_info, cancel_order_ack] = event;
  log::info<2>("cancel_order_ack={}, user_id={}, order_id={}, version={}"sv, cancel_order_ack, user_id, order_id, version);
  auto external_order_id = fmt::format("{}"sv, cancel_order_ack.order_id);  // alloc
  auto response = server::oms::Response{
      .request_type = RequestType::CANCEL_ORDER,
      .origin = Origin::EXCHANGE,
      .request_status = RequestStatus::ACCEPTED,
      .error = {},
      .text = {},
      .version = version,
      .request_id = {},
      .external_order_id = {},
      .quantity = NaN,
      .price = NaN,
  };
  auto order_update = server::oms::OrderUpdate{
      .account = account_.name,
      .exchange = shared_.settings.exchange,
      .symbol = cancel_order_ack.symbol,
      .side = map(cancel_order_ack.side),
      .position_effect = {},
      .margin_mode = MarginMode::PORTFOLIO,
      .max_show_quantity = NaN,
      .order_type = map(cancel_order_ack.type),
      .time_in_force = map(cancel_order_ack.time_in_force),
      .execution_instructions = {},
      .create_time_utc = {},
      .update_time_utc = {},
      .external_account = {},
      .external_order_id = external_order_id,
      .client_order_id = {},
      .order_status = map(cancel_order_ack.status),
      .error = {},
      .text = {},
      .quantity = cancel_order_ack.orig_qty,
      .price = cancel_order_ack.price,
      .stop_price = NaN,
      .leverage = NaN,
      .remaining_quantity = NaN,
      .traded_quantity = cancel_order_ack.executed_qty,
      .average_traded_price = NaN,
      .last_traded_quantity = NaN,
      .last_traded_price = NaN,
      .last_liquidity = {},
      .routing_id = {},
      .max_request_version = {},
      .max_response_version = {},
      .max_accepted_version = {},
      .update_type = UpdateType::INCREMENTAL,
      .sending_time_utc = {},
  };
  Trace event_2{trace_info, response};
  (*this)(event_2, user_id, order_id, order_update);
}

// cancel-all-orders

void OrderEntryPortfolio::cancel_all_open_orders(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  profile_.cancel_all_open_orders([&]() {
    if (!ready()) [[unlikely]] {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, cancel_all_orders] = event;
    auto send_ack = [&](auto &symbol) {
      auto cancel_all_orders_ack = CancelAllOrdersAck{
          .stream_id = stream_id_,
          .account = account_.name,
          .order_id = cancel_all_orders.order_id,
          .exchange = cancel_all_orders.exchange,
          .symbol = symbol,
          .side = cancel_all_orders.side,
          .origin = Origin::GATEWAY,
          .request_status = RequestStatus::FORWARDED,
          .error = {},
          .text = {},
          .request_id = request_id,
          .external_account = {},
          .number_of_affected_orders = {},
          .round_trip_latency = {},
          .user = {},
          .strategy_id = cancel_all_orders.strategy_id,
      };
      TraceInfo trace_info{event};
      Trace event_2{trace_info, cancel_all_orders_ack};
      shared_(event_2);
    };
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    for (auto &symbol : open_orders_symbols_) {
      if (!std::empty(cancel_all_orders.symbol) && symbol != cancel_all_orders.symbol) {
        continue;
      }
      auto now_utc = clock::get_realtime<std::chrono::milliseconds>();
      auto body = json::Encoder::cancel_all_open_orders_url(encode_buffer_, symbol, MarginMode::PORTFOLIO, recv_window, now_utc);
      auto query = account_.create_rest_signature_old_body(now_utc, body);
      auto headers = account_.get_rest_headers_old();
      auto request = web::rest::Request{
          .method = web::http::Method::DELETE,
          .path = shared_.api.papi.margin_all_open_orders,
          .query = query,
          .accept = web::http::Accept::APPLICATION_JSON,
          .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
          .headers = headers,
          .body = body,
          .quality_of_service = io::QualityOfService::IMMEDIATE,
      };
      log::debug("request={}"sv, request);
      auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
        TraceInfo trace_info;
        Trace event{trace_info, response};
        cancel_all_open_orders_ack(event, request_id);
      };
      (*connection_)(request_id, request, callback);
      send_ack(symbol);
    }
  });
}

void OrderEntryPortfolio::cancel_all_open_orders_ack(Trace<web::rest::Response> const &event, std::string_view const &request_id) {
  profile_.cancel_all_open_orders_ack([&]() {
    auto send_ack = [&](auto status, Error error, std::string_view const &text) {
      auto cancel_all_orders_ack = CancelAllOrdersAck{
          .stream_id = stream_id_,
          .account = account_.name,
          .order_id = {},
          .exchange = {},
          .symbol = {},
          .side = {},
          .origin = Origin::EXCHANGE,
          .request_status = status,
          .error = error,
          .text = text,
          .request_id = request_id,
          .external_account = {},
          .number_of_affected_orders = {},
          .round_trip_latency = {},
          .user = {},
          .strategy_id = {},
      };
      Trace event_2{event, cancel_all_orders_ack};
      shared_(event_2);
    };
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      switch (error) {
        using enum Error;
        case TOO_LATE_TO_MODIFY_OR_CANCEL:
          break;
        default:
          log::warn(R"(origin={}, error={}, status={}, text="{}")"sv, origin, error, status, text);
      }
      send_ack(RequestStatus::REJECTED, error, text);
    };
    auto handle_success = [&](auto &body) {
      json::CancelAllOpenOrdersAck cancel_all_open_orders_ack{body, decode_buffer_};
      Trace event_2{event, cancel_all_open_orders_ack};
      (*this)(event_2);
      send_ack(RequestStatus::ACCEPTED, {}, {});
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryPortfolio::operator()(Trace<json::CancelAllOpenOrdersAck> const &event) {
  auto &[trace_info, cancel_all_open_orders_ack] = event;
  log::info<2>("cancel_all_open_orders_ack={}"sv, cancel_all_open_orders_ack);
  for (auto &order : cancel_all_open_orders_ack.data) {
    if (std::empty(order.client_order_id)) {
      continue;
    }
    auto external_order_id = fmt::format("{}"sv, order.order_id);  // alloc
    auto order_update = server::oms::OrderUpdate{
        .account = account_.name,
        .exchange = shared_.settings.exchange,
        .symbol = order.symbol,
        .side = map(order.side),
        .position_effect = {},
        .margin_mode = MarginMode::PORTFOLIO,
        .max_show_quantity = NaN,
        .order_type = map(order.type),
        .time_in_force = map(order.time_in_force),
        .execution_instructions = {},
        .create_time_utc = order.time,
        .update_time_utc = order.update_time,
        .external_account = {},
        .external_order_id = external_order_id,
        .client_order_id = {},
        .order_status = map(order.status),
        .error = {},
        .text = {},
        .quantity = order.orig_qty,
        .price = order.price,
        .stop_price = order.stop_price,
        .leverage = NaN,
        .remaining_quantity = NaN,
        .traded_quantity = order.executed_qty,
        .average_traded_price = {},
        .last_traded_quantity = {},
        .last_traded_price = {},
        .last_liquidity = {},
        .routing_id = {},
        .max_request_version = {},
        .max_response_version = {},
        .max_accepted_version = {},
        .update_type = UpdateType::INCREMENTAL,
        .sending_time_utc = {},
    };
    // note! client_order_id is auto-generated by exchange
    shared_.update_order(order.orig_client_order_id, stream_id_, trace_info, order_update, []([[maybe_unused]] auto &order) {});
  }
}

// helpers

void OrderEntryPortfolio::process_response(web::rest::Response const &response, auto error_handler, auto success_handler) {
  try {
    auto [status, category, body] = response.result();
    switch (category) {
      using enum web::http::Category;
      case UNKNOWN:
      case INFORMATIONAL_RESPONSE:
        response.expect(web::http::Status::OK);  // throws
        break;
      case SUCCESS:
        success_handler(body);
        break;
      case REDIRECTION:
        log::fatal("Unexpected: URL is being redirected"sv);
      case CLIENT_ERROR:
        switch (status) {
          using enum web::http::Status;
          case FORBIDDEN:           // 403
            waf_limit_violation();  // note! this is *very* serious
            [[fallthrough]];
          case I_AM_A_TEAPOT:        // 418
          case TOO_MANY_REQUESTS: {  // 429
            auto retry_after = get_retry_after(response);
            if (retry_after.count()) {
              (*connection_).suspend(retry_after);
            }
            auto message = fmt::format("{}"sv, status);  // alloc
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::REQUEST_RATE_LIMIT_REACHED, message);
            break;
          }
          case CONFLICT:  // 409
            assert(false);
            [[fallthrough]];
          default: {
            json::ErrorError error{body};
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, json::guess_error(error.code), error.msg);
          }
        }
        break;
      case SERVER_ERROR: {
        auto message = fmt::format("{}"sv, status);  // alloc
        error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::UNKNOWN, message);
        break;
      }
    }
  } catch (server::oms::Exception &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(e.origin, e.status, e.error, e.what());
  } catch (NetworkError &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::GATEWAY, e.request_status(), e.error(), e.what());
  } catch (std::exception &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, e.what());
  }
}

template <typename... Args>
void OrderEntryPortfolio::operator()(Trace<server::oms::Response> const &event, uint8_t user_id, uint64_t order_id, Args &&...args) {
  auto &[trace_info, response] = event;
  if (shared_.update_order(user_id, order_id, stream_id_, trace_info, response, std::forward<Args>(args)..., []([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
  }
}

void OrderEntryPortfolio::operator()(Trace<server::oms::OrderUpdate> const &event, std::string_view const &client_order_id) {
  auto &[trace_info, order_update] = event;
  if (shared_.update_order(client_order_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

void OrderEntryPortfolio::waf_limit_violation() {
  if (shared_.settings.rest.terminate_on_403) {
    log::fatal("WAF limit violation"sv);
  } else {
    log::warn("WAF limit violation"sv);
    (*connection_).suspend(shared_.settings.rest.back_off_delay);
  }
}

}  // namespace binance
}  // namespace roq
