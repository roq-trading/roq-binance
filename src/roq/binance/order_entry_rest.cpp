/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/order_entry_rest.hpp"

#include <tuple>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/compare.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/utils/charconv/from_chars.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/server/oms/exceptions.hpp"

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
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto &account) {
  return fmt::format("{}:{}:{}"sv, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &settings, auto &context, auto &interface) {
  auto uri = settings.rest.uri;
  auto config = web::rest::Client::Config{
      // connection
      .interface = interface,
      .proxy = settings.rest.proxy,
      .uris = {&uri, 1},
      .host = settings.rest.host,
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
      .ping_path = settings.rest.ping_path,
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

auto get_download_trades_lookback(auto &settings, auto download_trades_is_first) {
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

OrderEntryREST::OrderEntryREST(
    OrderEntry::Handler &handler,
    io::Context &context,
    uint16_t stream_id,
    Account &account,
    Shared &shared,
    Request &request,
    bool master,
    std::string_view const &interface)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.name)}, master_{master},
      connection_{create_connection(*this, shared.settings, context, interface)}, decode_buffer_(shared.settings.misc.decode_buffer_size),
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
}

void OrderEntryREST::operator()(Event<Start> const &) {
  (*connection_).start();
}

void OrderEntryREST::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void OrderEntryREST::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  refresh_listen_key(now);
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

void OrderEntryREST::operator()(metrics::Writer &writer) {
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

void OrderEntryREST::operator()(Event<Disconnected> const &event) {
  auto user_id = event.message_info.source;
  account_.cancel_order_request_buffer_[user_id].reset();
}

uint16_t OrderEntryREST::operator()(Event<CreateOrder> const &event, server::oms::Order const &order, std::string_view const &request_id) {
  auto &message_info = event.message_info;
  auto &tmp = account_.cancel_order_request_buffer_[message_info.source];
  if (!tmp) {
    new_order(event, order, request_id);
  } else {
    // cancel + replace
    typename std::remove_cvref_t<decltype(tmp)> tmp2;
    tmp.swap(tmp2);
    // XXX HANS do we get error on cancel if we can't send?
    cancel_replace_order(*tmp2, event, order, request_id);
  }
  return stream_id_;
}

uint16_t OrderEntryREST::operator()(
    Event<ModifyOrder> const &,
    server::oms::Order const &,
    [[maybe_unused]] std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id) {
  throw server::oms::NotSupported{"not supported"sv};
  return stream_id_;
}

uint16_t OrderEntryREST::operator()(
    Event<CancelOrder> const &event, server::oms::Order const &order, std::string_view const &request_id, std::string_view const &previous_request_id) {
  auto &[message_info, cancel_order] = event;
  auto &tmp = account_.cancel_order_request_buffer_[message_info.source];
  if (tmp) {
    throw server::oms::NotSupported{"not supported"sv};
  }
  if (message_info.is_last) {
    (*this).cancel_order(event, order, request_id, previous_request_id);
  } else {
    // cancel + replace
    auto cancel_order_request = server::cache::CancelOrderRequest{
        .cancel_order = cache::CancelOrder{cancel_order},
        .request_id = request_id,
        .previous_request_id = previous_request_id,
    };
    tmp = std::make_unique<server::cache::CancelOrderRequest>(std::move(cancel_order_request));
  }
  return stream_id_;
}

uint16_t OrderEntryREST::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  cancel_all_open_orders(event, request_id);
  return stream_id_;
}

void OrderEntryREST::operator()(Trace<web::rest::Client::Connected> const &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void OrderEntryREST::operator()(Trace<web::rest::Client::Disconnected> const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading()) {
    download_.reset();
  }
  download_account_ = false;
  download_orders_ = false;
}

void OrderEntryREST::operator()(Trace<web::rest::Client::Header> const &event) {
  auto &header = event.value;
  if (utils::case_insensitive_compare(header.name, "x-mbx-used-weight-1m"sv) == 0) {
    try {
      auto value = utils::charconv::from_string_relaxed<int64_t>(header.value);
      rate_limiter_.requests_1m.set(value);
    } catch (RuntimeError &) {
      log::warn<5>(R"(Failed to parse text="{}")"sv, header.value);
    }
  }
}

void OrderEntryREST::operator()(Trace<web::rest::Client::Latency> const &event) {
  auto &[trace_info, latency] = event;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = account_.name,
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntryREST::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    auto stream_status = StreamStatus{
        .stream_id = stream_id_,
        .account = account_.name,
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::HTTP,
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

uint32_t OrderEntryREST::download(OrderEntryState state) {
  switch (state) {
    using enum OrderEntryState;
    case UNDEFINED:
      assert(false);
      break;
    case LISTEN_KEY:
      if (master_) {
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

void OrderEntryREST::get_listen_key() {
  profile_.listen_key([&]() {
    auto headers = account_.create_headers();
    auto request = web::rest::Request{
        .method = web::http::Method::POST,
        .path = shared_.api.simple.user_data_stream,
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
    (*connection_)("listen_key"sv, request, callback);
  });
}

void OrderEntryREST::get_listen_key_ack(Trace<web::rest::Response> const &event) {
  auto constexpr const STATE = OrderEntryState::LISTEN_KEY;
  profile_.listen_key_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::ListenKey listen_key{body};
      Trace event_2{event, listen_key};
      (*this)(event_2);
      download_.check_relaxed(STATE);
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntryREST::operator()(Trace<json::ListenKey> const &event) {
  auto &[trace_info, listen_key] = event;
  log::info<2>("listen_key={}"sv, listen_key);
  bool initial = std::empty(listen_key_);
  if (utils::update(listen_key_, listen_key.listen_key)) {
    if (initial) {
      log::info<1>(R"(Listen key has been acquired (value="{}"))"sv, listen_key_);
      auto listen_key_update = ListenKeyUpdate{
          .account = account_.name,
          .margin_mode = {},
          .listen_key = listen_key.listen_key,
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

void OrderEntryREST::get_account() {
  profile_.account([&]() {
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_query(now);
    auto headers = account_.create_headers();
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = shared_.api.simple.account,
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
    (*connection_)("account"sv, request, callback);
  });
}

void OrderEntryREST::get_account_ack(Trace<web::rest::Response> const &event) {
  profile_.account_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::Account account{body, decode_buffer_};
      Trace event_2{event, account};
      (*this)(event_2);
      request_.respond_account = clock::get_system();  // completion
      download_account_ = false;
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      request_.respond_account = clock::get_system();  // completion
      download_account_ = false;
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntryREST::operator()(Trace<json::Account> const &event) {
  auto &[trace_info, account] = event;
  log::info<2>("account={}"sv, account);
  for (auto &item : account.balances) {
    auto funds_update = FundsUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .currency = item.asset,
        .margin_mode = {},
        .balance = item.free,
        .hold = item.locked,
        .external_account = {},
        .update_type = UpdateType::SNAPSHOT,
        .exchange_time_utc = account.update_time,
        .sending_time_utc = account.update_time,
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

// orders

void OrderEntryREST::get_open_orders() {
  profile_.open_orders([&]() {
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_query(now);
    auto headers = account_.create_headers();
    auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
    encode_buffer_.clear();
    fmt::format_to(
        std::back_inserter(encode_buffer_),
        R"({{)"
        R"("timestamp":{})"
        R"(}})"sv,
        timestamp.count());
    std::string body{std::data(encode_buffer_), std::size(encode_buffer_)};
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = shared_.api.simple.open_orders,
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
    (*connection_)("open_orders"sv, request, callback);
  });
}

void OrderEntryREST::get_open_orders_ack(Trace<web::rest::Response> const &event) {
  profile_.open_orders_ack([&]() {
    auto handle_success = [&](auto &body) {
      log::debug("body={}"sv, body);
      json::OpenOrders open_orders{body, decode_buffer_};
      Trace event_2{event, open_orders};
      (*this)(event_2, false);
      request_.respond_orders = clock::get_system();  // completion
      download_orders_ = false;
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      request_.respond_orders = clock::get_system();  // completion
      download_orders_ = false;
    };
    process_response(event, handle_success, handle_error);
  });
}

// margin:
// body=[{"symbol":"BTCUSDT","orderId":43109407062,"clientOrderId":"web_26025fbb1554418bb655a2a39e54824c","price":"110000","origQty":"0.0001","executedQty":"0","cummulativeQuoteQty":"0","status":"NEW","timeInForce":"GTC","type":"LIMIT","side":"SELL","stopPrice":"0","icebergQty":"0","time":1747403389646,"updateTime":1747403389646,"isWorking":true,"isIsolated":false,"selfTradePreventionMode":"EXPIRE_MAKER"}]

void OrderEntryREST::operator()(Trace<json::OpenOrders> const &event, bool is_margin) {
  auto &[trace_info, open_orders] = event;
  for (auto &order : open_orders.data) {
    log::info<2>("order={}"sv, order);
    if (std::empty(order.client_order_id)) {
      continue;
    }
    open_orders_symbols_.emplace(order.symbol);
    auto margin_mode = [&]() -> MarginMode {
      if (is_margin) {
        return order.is_isolated ? MarginMode::ISOLATED : MarginMode::CROSS;
      }
      return {};
    }();
    auto external_order_id = fmt::format("{}"sv, order.order_id);  // alloc
    auto order_update = server::oms::OrderUpdate{
        .account = account_.name,
        .exchange = shared_.settings.exchange,
        .symbol = order.symbol,
        .side = map(order.side),
        .position_effect = {},
        .margin_mode = margin_mode,
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
        .quantity = order.orig_qty,
        .price = order.price,
        .stop_price = order.stop_price,
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
        .update_type = UpdateType::SNAPSHOT,
        .sending_time_utc = {},
    };
    Trace event_2{trace_info, order_update};
    (*this)(event_2, order.client_order_id);
  }
}

// trades

void OrderEntryREST::get_trades() {
  profile_.trades([&]() {
    auto &symbols = shared_.settings.download.symbols;
    for (auto &symbol : symbols) {
      auto now = clock::get_realtime<std::chrono::milliseconds>();
      auto lookback = get_download_trades_lookback(shared_.settings, download_trades_is_first_);
      log::info<1>("Download trades: lookback={}"sv, lookback);
      auto headers = account_.create_headers();
      auto body = json::my_trades(encode_buffer_, symbol, lookback, shared_.settings.download.trades_limit, now);
      auto query = account_.create_query(now, body);
      auto request = web::rest::Request{
          .method = web::http::Method::GET,
          .path = shared_.api.simple.my_trades,
          .query = query,
          .accept = web::http::Accept::APPLICATION_JSON,
          .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
          .headers = headers,
          .body = body,
          .quality_of_service = {},
      };
      auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
        TraceInfo trace_info;
        Trace event{trace_info, response};
        get_trades_ack(event);
      };
      (*connection_)("my-trades"sv, request, callback);
    }
  });
}

void OrderEntryREST::get_trades_ack(Trace<web::rest::Response> const &event) {
  profile_.trades_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::Trades trades{body, decode_buffer_};
      Trace event_2{event, trades};
      (*this)(event_2);
      request_.respond_trades = clock::get_system();  // completion
      download_trades_ = false;
      download_trades_is_first_ = false;
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      request_.respond_trades = clock::get_system();  // completion
      download_trades_ = false;
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntryREST::operator()(Trace<json::Trades> const &event) {
  auto &[trace_info, trades] = event;
  for (auto &trade : trades.data) {
    log::info<2>("trade={}"sv, trade);
    /*
    if (std::empty(order.client_order_id))
      continue;
    open_orders_symbols_.emplace(order.symbol);
    auto side = json::map(order.side);
    auto order_type = json::map(order.type);
    auto time_in_force = json::map(order.time_in_force);
    auto external_order_id = fmt::format("{}"sv, order.order_id);  // alloc
    auto order_status = json::map(order.status);
    auto order_update = server::oms::OrderUpdate{
        .account = account_.name,
        .exchange = shared_.settings.exchange,
        .symbol = order.symbol,
        .side = side,
        .position_effect = {},
        .margin_mode={},
        .max_show_quantity = NaN,
        .order_type = order_type,
        .time_in_force = time_in_force,
        .execution_instructions = {},
        .create_time_utc = order.time,
        .update_time_utc = order.update_time,
        .external_account = {},
        .external_order_id = external_order_id,
        .client_order_id = {},
        .order_status = order_status,
        .quantity = order.orig_qty,
        .price = order.price,
        .stop_price = order.stop_price,
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
        .update_type = UpdateType::SNAPSHOT,
        .sending_time_utc = {},
    };
    Trace event_2{trace_info, order_update};
    (*this)(event_2, order.client_order_id);
    */
  }
}

// ...

void OrderEntryREST::refresh_listen_key(std::chrono::nanoseconds now) {
  if (!ready_) {
    return;
  }
  if (listen_key_refresh_ == listen_key_refresh_.zero() || now < listen_key_refresh_) {
    return;
  }
  log::info<1>("Refreshing listen key..."sv);
  listen_key_refresh_ = now + shared_.settings.rest.listen_key_refresh;
  get_listen_key();
}

// new-order

void OrderEntryREST::new_order(Event<CreateOrder> const &event, server::oms::Order const &order, std::string_view const &request_id) {
  profile_.new_order([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, create_order] = event;
    open_orders_symbols_.emplace(create_order.symbol);
    auto &create_order_template = shared_.get_create_order_template(create_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto body = json::new_order(encode_buffer_, create_order, order, request_id, create_order_template, recv_window);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_query(now, body);
    auto headers = account_.create_headers();
    auto path = [&]() -> std::string_view {
      switch (create_order.margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          return shared_.api.simple.order;
        case ISOLATED:
        case CROSS:
          return shared_.api.simple.margin_order;
        case PORTFOLIO:
          throw server::oms::Rejected{Origin::GATEWAY, Error::INVALID_MARGIN_MODE, "internal error"sv};
      };
      log::fatal("Unexpected"sv);
    }();
    auto request = web::rest::Request{
        .method = web::http::Method::POST,
        .path = path,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
        .headers = headers,
        .body = body,
        .quality_of_service = io::QualityOfService::IMMEDIATE,
    };
    auto callback = [this, user_id = message_info.source, order_id = create_order.order_id]([[maybe_unused]] auto &request_id, auto &response) {
      uint32_t version = 1;
      TraceInfo trace_info;
      Trace event{trace_info, response};
      new_order_ack(event, user_id, order_id, version);
    };
    (*connection_)(request_id, request, callback);
  });
}

void OrderEntryREST::new_order_ack(Trace<web::rest::Response> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  profile_.new_order_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::NewOrder new_order{body, decode_buffer_};
      Trace event_2{event, new_order};
      (*this)(event_2, user_id, order_id, version);
    };
    auto handle_error = [&](auto origin, auto status, auto error, auto text) {
      auto response = server::oms::Response{
          .request_type = RequestType::CREATE_ORDER,
          .origin = origin,
          .request_status = status,
          .error = error,
          .text = text,
          .version = version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, user_id, order_id);
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntryREST::operator()(Trace<json::NewOrder> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  auto &[trace_info, new_order] = event;
  log::info<2>("new_order={}, user_id={}, order_id={}, version={}"sv, new_order, user_id, order_id, version);
  auto external_order_id = fmt::format("{}"sv, new_order.order_id);  // alloc
  auto order_status = map(new_order.status).template get<OrderStatus>();
  // LIMIT_MAKER orders do not return any order state + we only end up here if we receive HTTP status OK
  if (order_status == OrderStatus{}) {
    order_status = OrderStatus::WORKING;
  }
  auto remaining_quantity = new_order.orig_qty - new_order.executed_qty;
  auto average_traded_price = utils::is_zero(new_order.executed_qty) ? NaN : (new_order.cummulative_quote_qty / new_order.executed_qty);
  auto last_traded_quantity = double{0.0};  // note! could also use new_order.executed_qty
  auto tmp = double{0.0};
  for (auto &item : new_order.fills) {
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
      .quantity = NaN,
      .price = NaN,
  };
  auto order_update = server::oms::OrderUpdate{
      .account = account_.name,
      .exchange = shared_.settings.exchange,
      .symbol = new_order.symbol,
      .side = map(new_order.side),
      .position_effect = {},
      .margin_mode = {},
      .max_show_quantity = NaN,
      .order_type = map(new_order.type),
      .time_in_force = map(new_order.time_in_force),
      .execution_instructions = {},
      .create_time_utc = {},
      .update_time_utc = new_order.transact_time,
      .external_account = {},
      .external_order_id = external_order_id,
      .client_order_id = {},
      .order_status = order_status,
      .quantity = new_order.orig_qty,
      .price = new_order.price,
      .stop_price = NaN,
      .remaining_quantity = remaining_quantity,
      .traded_quantity = new_order.executed_qty,
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

// cancel-replace-order

void OrderEntryREST::cancel_replace_order(
    server::cache::CancelOrderRequest const &cancel_order_request,
    Event<CreateOrder> const &event,
    server::oms::Order const &order,
    std::string_view const &request_id) {
  profile_.cancel_replace_order([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    if (shared_.find_order(event.message_info.source, cancel_order_request.cancel_order.order_id, [&](auto &cancel_order) {
          auto &[message_info, create_order] = event;
          auto &cancel_order_template = shared_.get_cancel_order_template(cancel_order_request.cancel_order.request_template);
          auto &create_order_template = shared_.get_create_order_template(create_order.request_template);
          auto body = json::cancel_replace_order(
              encode_buffer_,
              cancel_order_request.request_id,
              cancel_order_request.previous_request_id,
              cancel_order.external_order_id,
              create_order,
              order,
              request_id,
              cancel_order_template,
              create_order_template,
              utils::safe_cast(shared_.settings.rest.order_recv_window),
              shared_.settings.oms.cancel_replace_stop_on_failure);
          auto now = clock::get_realtime<std::chrono::milliseconds>();
          auto query = account_.create_query(now, body);
          auto headers = account_.create_headers();
          auto request = web::rest::Request{
              .method = web::http::Method::POST,
              .path = shared_.api.simple.order_cancel_replace,
              .query = query,
              .accept = web::http::Accept::APPLICATION_JSON,
              .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
              .headers = headers,
              .body = body,
              .quality_of_service = io::QualityOfService::IMMEDIATE,
          };
          auto callback = [this,
                           user_id = message_info.source,
                           cancel_order_id = cancel_order_request.cancel_order.order_id,
                           cancel_version = cancel_order_request.cancel_order.version,
                           create_order_id = create_order.order_id]([[maybe_unused]] auto &request_id, auto &response) {
            TraceInfo trace_info;
            Trace event{trace_info, response};
            cancel_replace_order_ack(event, user_id, cancel_order_id, cancel_version, create_order_id, uint32_t{1});
          };
          (*connection_)(request_id, request, callback);
        })) {
    } else {
      throw server::oms::Rejected{Origin::GATEWAY, Error::CONDITIONAL_REQUEST_HAS_FAILED, "internal error"sv};
    }
  });
}

/*
{"code":-1102,"msg":"Mandatory parameter 'timeInForce' was not sent, was empty/null, or malformed."}
{"code":-1013,"msg":"Filter failure: MIN_NOTIONAL"}
{"code":-1102,"msg":"Mandatory parameter 'quantity' was not sent, was empty/null, or malformed."}
*/
/*
symbol=BTCUSDT&side=BUY&type=LIMIT&cancelReplaceMode=STOP_ON_FAILURE&timeInForce=GTC&quantity=0.00100&price=23144.46&cancelOrigClientOrderId=CgAC6QMAAQAAvgAMirEt&newClientOrderId=gwAC6QMAAgAAXFo-irEt&recvWindow=5000
{
"cancelResult":"SUCCESS",
"newOrderResult":"SUCCESS",
"cancelResponse":{
"symbol":"BTCUSDT",
"origClientOrderId":"CgAC6QMAAQAAvgAMirEt",
"orderId":12166372890,
"orderListId":-1,
"clientOrderId":"PZSf3aT26PBaSacwoUWU9g",
"price":"23238.21000000",
"origQty":"0.00100000",
"executedQty":"0.00000000",
"cummulativeQuoteQty":"0.00000000",
"status":"CANCELED",
"timeInForce":"GTC",
"type":"LIMIT",
"side":"BUY"
},
"newOrderResponse":{
"symbol":"BTCUSDT",
"orderId":12166375153,
"orderListId":-1,
"clientOrderId":"gwAC6QMAAgAAXFo-irEt",
"transactTime":1659699751935,
"price":"23144.46000000",
"origQty":"0.00100000",
"executedQty":"0.00000000",
"cummulativeQuoteQty":"0.00000000",
"status":"NEW",
"timeInForce":"GTC",
"type":"LIMIT",
"side":"BUY",
"fills":[]
}
}
*/
void OrderEntryREST::cancel_replace_order_ack(
    Trace<web::rest::Response> const &event,
    uint8_t user_id,
    uint64_t cancel_order_id,
    uint32_t cancel_version,
    uint64_t create_order_id,
    uint32_t create_version) {
  profile_.cancel_replace_order_ack([&]() {
    auto &trace_info = event.trace_info;
    auto &response = event.value;
    try {
      auto [status, category, body] = response.result();
      test(status);
      switch (category) {
        using enum web::http::Category;
        case SUCCESS: {  // 2xx
          json::CancelReplaceOrder cancel_replace_order{body, decode_buffer_};
          Trace event{trace_info, cancel_replace_order};
          (*this)(event, user_id, cancel_order_id, cancel_version, create_order_id, create_version);
          break;
        }
        case CLIENT_ERROR:    // 4xx
        case SERVER_ERROR: {  // 5xx
          auto parse = [&]() {
            json::CancelReplaceOrderError cancel_replace_order_error{body, decode_buffer_};
            Trace event{trace_info, cancel_replace_order_error};
            (*this)(event, user_id, cancel_order_id, cancel_version, create_order_id, create_version);
          };
          dispatch_error_2(response, category, status, parse, [&]([[maybe_unused]] auto status, auto error, auto text) {
            {  // cancel
              auto response = server::oms::Response{
                  .request_type = RequestType::CANCEL_ORDER,
                  .origin = Origin::EXCHANGE,
                  .request_status = status,
                  .error = error,
                  .text = text,
                  .version = cancel_version,
                  .request_id = {},
                  .quantity = NaN,
                  .price = NaN,
              };
              if (shared_.update_order(user_id, cancel_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
              } else {
                log::warn("Did not find order: user_id={}, order_id={}, version={}"sv, user_id, cancel_order_id, cancel_version);
              }
            }
            {  // create
              auto response = server::oms::Response{
                  .request_type = RequestType::CREATE_ORDER,
                  .origin = Origin::EXCHANGE,
                  .request_status = status,
                  .error = error,
                  .text = text,
                  .version = create_version,
                  .request_id = {},
                  .quantity = NaN,
                  .price = NaN,
              };
              if (shared_.update_order(user_id, create_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
              } else {
                log::warn("Did not find order: user_id={}, order_id={}, version={}"sv, user_id, create_order_id, create_version);
              }
            }
          });
          break;
        }
        default:
          response.expect(web::http::Status::OK);  // throws
      }
    } catch (NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      {  // cancel
        auto response = server::oms::Response{
            .request_type = RequestType::CANCEL_ORDER,
            .origin = Origin::GATEWAY,
            .request_status = e.request_status(),
            .error = e.error(),
            .text = e.what(),
            .version = cancel_version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        if (shared_.update_order(user_id, cancel_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
        } else {
          log::warn("Did not find order: user_id={}, order_id={}, version={}"sv, user_id, cancel_order_id, cancel_version);
        }
      }
      {  // create
        auto response = server::oms::Response{
            .request_type = RequestType::CREATE_ORDER,
            .origin = Origin::GATEWAY,
            .request_status = e.request_status(),
            .error = e.error(),
            .text = e.what(),
            .version = create_version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        if (shared_.update_order(user_id, create_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
        } else {
          log::warn("Did not find order: user_id={}, order_id={}, version={}"sv, user_id, create_order_id, create_version);
        }
      }
    }
  });
}

void OrderEntryREST::operator()(
    Trace<json::CancelReplaceOrder> const &event,
    uint8_t user_id,
    uint64_t cancel_order_id,
    uint32_t cancel_version,
    uint64_t create_order_id,
    uint32_t create_version) {
  auto &[trace_info, cancel_replace_order] = event;
  log::info<2>(
      "cancel_replace_order={}, "
      "user_id={}, cancel_order_id={}, cancel_version={}, create_order_id={}, create_version={}"sv,
      cancel_replace_order,
      user_id,
      cancel_order_id,
      cancel_version,
      create_order_id,
      create_version);
  switch (cancel_replace_order.cancel_result) {
    using enum json::SuccessOrFailure::type_t;
    case UNDEFINED_INTERNAL:
    case UNKNOWN_INTERNAL:
      log::warn("Unexpected"sv);
      break;
    case SUCCESS: {
      auto &cancel_order = cancel_replace_order.cancel_response;
      auto external_order_id = fmt::format("{}"sv, cancel_order.order_id);  // alloc
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = cancel_version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      auto order_update = server::oms::OrderUpdate{
          .account = account_.name,
          .exchange = shared_.settings.exchange,
          .symbol = cancel_order.symbol,
          .side = map(cancel_order.side),
          .position_effect = {},
          .margin_mode = {},
          .max_show_quantity = NaN,
          .order_type = map(cancel_order.type),
          .time_in_force = map(cancel_order.time_in_force),
          .execution_instructions = {},
          .create_time_utc = {},
          .update_time_utc = {},
          .external_account = {},
          .external_order_id = external_order_id,
          .client_order_id = {},
          .order_status = map(cancel_order.status),
          .quantity = cancel_order.orig_qty,
          .price = cancel_order.price,
          .stop_price = NaN,
          .remaining_quantity = NaN,
          .traded_quantity = cancel_order.executed_qty,
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
      if (shared_.update_order(user_id, cancel_order_id, stream_id_, trace_info, response, order_update, []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("Did not find order: user_id={}, order_id={}, version={}"sv, user_id, cancel_order_id, cancel_version);
      }
      break;
    }
    case FAILURE:
    case NOT_ATTEMPTED: {
      auto &cancel_order = cancel_replace_order.cancel_response;
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::REJECTED,
          .error = json::guess_error(cancel_order.code),
          .text = cancel_order.msg,
          .version = cancel_version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(user_id, cancel_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("Did not find order: user_id={}, order_id={}, version={}"sv, user_id, cancel_order_id, cancel_version);
      }
      break;
    }
  }
  switch (cancel_replace_order.new_order_result) {
    using enum json::SuccessOrFailure::type_t;
    case UNDEFINED_INTERNAL:
    case UNKNOWN_INTERNAL:
      log::warn("Unexpected"sv);
      break;
    case SUCCESS: {
      auto &new_order = cancel_replace_order.new_order_response;
      auto external_order_id = fmt::format("{}"sv, new_order.order_id);  // alloc
      auto response = server::oms::Response{
          .request_type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = create_version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      auto order_update = server::oms::OrderUpdate{
          .account = account_.name,
          .exchange = shared_.settings.exchange,
          .symbol = new_order.symbol,
          .side = map(new_order.side),
          .position_effect = {},
          .margin_mode = {},
          .max_show_quantity = NaN,
          .order_type = map(new_order.type),
          .time_in_force = map(new_order.time_in_force),
          .execution_instructions = {},
          .create_time_utc = {},
          .update_time_utc = {},
          .external_account = {},
          .external_order_id = external_order_id,
          .client_order_id = {},
          .order_status = map(new_order.status),
          .quantity = new_order.orig_qty,
          .price = new_order.price,
          .stop_price = NaN,
          .remaining_quantity = NaN,
          .traded_quantity = new_order.executed_qty,
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
      if (shared_.update_order(user_id, create_order_id, stream_id_, trace_info, response, order_update, []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("Did not find order: user_id={}, order_id={}, version={}"sv, user_id, create_order_id, create_version);
      }
      break;
    }
    case FAILURE:
    case NOT_ATTEMPTED: {
      auto &new_order = cancel_replace_order.new_order_response;
      auto response = server::oms::Response{
          .request_type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::REJECTED,
          .error = json::guess_error(new_order.code),
          .text = new_order.msg,
          .version = create_version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(user_id, create_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("Did not find order: user_id={}, order_id={}, version={}"sv, user_id, create_order_id, create_version);
      }
      break;
    }
  }
}

void OrderEntryREST::operator()(
    Trace<json::CancelReplaceOrderError> const &event,
    uint8_t user_id,
    uint64_t cancel_order_id,
    uint32_t cancel_version,
    uint64_t create_order_id,
    uint32_t create_version) {
  auto &[trace_info, cancel_replace_order_error] = event;
  log::info<2>(
      "cancel_replace_order_error={}, "
      "user_id={}, cancel_order_id={}, cancel_version={}, create_order_id={}, create_version={}"sv,
      cancel_replace_order_error,
      user_id,
      cancel_order_id,
      cancel_version,
      create_order_id,
      create_version);
  assert(cancel_replace_order_error.code != 0);
  Trace event_2{trace_info, cancel_replace_order_error.data};
  (*this)(event_2, user_id, cancel_order_id, cancel_version, create_order_id, create_version);
}

// cancel-order

void OrderEntryREST::cancel_order(
    Event<CancelOrder> const &event, server::oms::Order const &order, std::string_view const &request_id, std::string_view const &previous_request_id) {
  profile_.cancel_order([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, cancel_order] = event;
    auto &cancel_order_template = shared_.get_cancel_order_template(cancel_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto body = json::cancel_order(encode_buffer_, cancel_order, order, request_id, previous_request_id, cancel_order_template, recv_window);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_query(now, body);
    auto headers = account_.create_headers();
    auto path = [&]() -> std::string_view {
      switch (order.margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          return shared_.api.simple.order;
        case ISOLATED:
        case CROSS:
          return shared_.api.simple.margin_order;
        case PORTFOLIO:
          throw server::oms::Rejected{Origin::GATEWAY, Error::INVALID_MARGIN_MODE, "internal error"sv};
      };
      log::fatal("Unexpected"sv);
    }();
    auto request = web::rest::Request{
        .method = web::http::Method::DELETE,
        .path = path,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
        .headers = headers,
        .body = body,
        .quality_of_service = io::QualityOfService::IMMEDIATE,
    };
    auto callback = [this, user_id = message_info.source, order_id = cancel_order.order_id, version = cancel_order.version](
                        [[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      cancel_order_ack(event, user_id, order_id, version);
    };
    (*connection_)(request_id, request, callback);
  });
}

void OrderEntryREST::cancel_order_ack(Trace<web::rest::Response> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  profile_.cancel_order_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::CancelOrder cancel_order{body};
      Trace event_2{event, cancel_order};
      (*this)(event_2, user_id, order_id, version);
    };
    auto handle_error = [&](auto origin, auto status, auto error, auto text) {
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = origin,
          .request_status = status,
          .error = error,
          .text = text,
          .version = version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, user_id, order_id);
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntryREST::operator()(Trace<json::CancelOrder> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  auto &[trace_info, cancel_order] = event;
  log::info<2>("cancel_order={}, user_id={}, order_id={}, version={}"sv, cancel_order, user_id, order_id, version);
  auto external_order_id = fmt::format("{}"sv, cancel_order.order_id);  // alloc
  auto response = server::oms::Response{
      .request_type = RequestType::CANCEL_ORDER,
      .origin = Origin::EXCHANGE,
      .request_status = RequestStatus::ACCEPTED,
      .error = {},
      .text = {},
      .version = version,
      .request_id = {},
      .quantity = NaN,
      .price = NaN,
  };
  auto order_update = server::oms::OrderUpdate{
      .account = account_.name,
      .exchange = shared_.settings.exchange,
      .symbol = cancel_order.symbol,
      .side = map(cancel_order.side),
      .position_effect = {},
      .margin_mode = {},
      .max_show_quantity = NaN,
      .order_type = map(cancel_order.type),
      .time_in_force = map(cancel_order.time_in_force),
      .execution_instructions = {},
      .create_time_utc = {},
      .update_time_utc = {},
      .external_account = {},
      .external_order_id = external_order_id,
      .client_order_id = {},
      .order_status = map(cancel_order.status),
      .quantity = cancel_order.orig_qty,
      .price = cancel_order.price,
      .stop_price = NaN,
      .remaining_quantity = NaN,
      .traded_quantity = cancel_order.executed_qty,
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

void OrderEntryREST::cancel_all_open_orders(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  profile_.cancel_all_open_orders([&]() {
    if (!ready()) [[unlikely]] {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &cancel_all_orders = event.value;
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
      auto body = json::cancel_all_open_orders(encode_buffer_, symbol, recv_window);
      auto now = clock::get_realtime<std::chrono::milliseconds>();
      auto query = account_.create_query(now, body);
      auto headers = account_.create_headers();
      auto request = web::rest::Request{
          .method = web::http::Method::DELETE,
          .path = shared_.api.simple.open_orders,
          .query = query,
          .accept = web::http::Accept::APPLICATION_JSON,
          .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
          .headers = headers,
          .body = body,
          .quality_of_service = io::QualityOfService::IMMEDIATE,
      };
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

void OrderEntryREST::cancel_all_open_orders_ack(Trace<web::rest::Response> const &event, std::string_view const &request_id) {
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
    auto handle_success = [&](auto &body) {
      json::CancelAllOpenOrders cancel_all_open_orders{body, decode_buffer_};
      Trace event_2{event, cancel_all_open_orders};
      (*this)(event_2);
      send_ack(RequestStatus::ACCEPTED, {}, {});
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      switch (error) {
        using enum Error;
        case TOO_LATE_TO_MODIFY_OR_CANCEL:
          break;
        default:
          log::warn(R"(error={}, text="{}")"sv, error, text);
      }
      send_ack(RequestStatus::REJECTED, error, text);
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntryREST::operator()(Trace<json::CancelAllOpenOrders> const &event) {
  auto &[trace_info, cancel_all_open_orders] = event;
  log::info<2>("cancel_all_open_orders={}"sv, cancel_all_open_orders);
  for (auto &order : cancel_all_open_orders.data) {
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
        .margin_mode = {},
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
        .quantity = order.orig_qty,
        .price = order.price,
        .stop_price = order.stop_price,
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
    shared_.update_order(order.client_order_id, stream_id_, trace_info, order_update, []([[maybe_unused]] auto &order) {});
  }
}

template <typename SuccessHandler, typename ErrorHandler>
void OrderEntryREST::process_response(web::rest::Response const &response, SuccessHandler success_handler, ErrorHandler error_handler) {
  try {
    auto [status, category, body] = response.result();
    switch (category) {
      using enum web::http::Category;
      case SUCCESS:  // 2xx
        success_handler(body);
        break;
      case CLIENT_ERROR:  // 4xx
        switch (status) {
          using enum web::http::Status;
          case FORBIDDEN:           // 403
            waf_limit_violation();  // note! this is *very* serious
            [[fallthrough]];
          case I_AM_A_TEAPOT:        // 418
          case TOO_MANY_REQUESTS: {  // 429
            auto text = fmt::format("{}"sv, status);
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::REQUEST_RATE_LIMIT_REACHED, text);
            break;
          }
          case CONFLICT:  // 409
            assert(false);
            [[fallthrough]];
          default: {
            json::Error error{body};
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, json::guess_error(error.code), error.msg);
          }
        }
        break;
      case SERVER_ERROR: {  // 5xx
        auto text = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, text);
        break;
      }
      default:
        response.expect(web::http::Status::OK);  // throws
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
void OrderEntryREST::operator()(Trace<server::oms::Response> const &event, uint8_t user_id, uint64_t order_id, Args &&...args) {
  auto &[trace_info, response] = event;
  if (shared_.update_order(user_id, order_id, stream_id_, trace_info, response, std::forward<Args>(args)..., []([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
  }
}

void OrderEntryREST::operator()(Trace<server::oms::OrderUpdate> const &event, std::string_view const &client_order_id) {
  auto &[trace_info, order_update] = event;
  if (shared_.update_order(client_order_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

// note! used by cancel-replace
template <typename Parse, typename Callback>
void OrderEntryREST::dispatch_error_2(
    web::rest::Response const &response, web::http::Category category, web::http::Status status, Parse parse, Callback callback) {
  switch (category) {
    using enum web::http::Category;
    case UNKNOWN:
      break;
    case INFORMATIONAL_RESPONSE:
    case SUCCESS:
    case REDIRECTION:
      assert(false);
      break;
    case CLIENT_ERROR:  // 4xx
      try {
        // HTTP 4XX return codes are used for malformed requests; the issue is on the sender's side.
        // HTTP 403 return code is used when the WAF Limit (Web Application Firewall) has been violated.
        // HTTP 409 return code is used when a cancelReplace order partially succeeds. (e.g. if the
        //   cancellation of the order fails but the new order placement succeeds.)
        // HTTP 418 return code is used when an IP has been auto-banned for continuing to send requests
        //   after receiving 429 codes.
        // HTTP 429 return code is used when breaking a request rate limit.
        switch (status) {
          using enum web::http::Status;
          case FORBIDDEN:            // 403
          case I_AM_A_TEAPOT:        // 418
          case TOO_MANY_REQUESTS: {  // 429
            auto retry_after = get_retry_after(response);
            if (retry_after.count()) {
              (*connection_).suspend(retry_after);
            }
            auto text = fmt::format("{}"sv, status);
            callback(RequestStatus::REJECTED, Error::REQUEST_RATE_LIMIT_REACHED, text);
            break;
          }
          case CONFLICT:  // 409
            assert(false);
            [[fallthrough]];
          default:
            parse();
        }
      } catch (std::exception &e) {  // parse error
        callback(RequestStatus::ERROR, Error::UNKNOWN, e.what());
      }
      break;
    case SERVER_ERROR: {  // 5xx
      // HTTP 5XX return codes are used for internal errors; the issue is on Binance's side.
      //   It is important to NOT treat this as a failure operation; the execution status is UNKNOWN
      //   and could have been a success.
      auto text = fmt::format("{}"sv, status);
      callback(RequestStatus::ERROR, Error::UNKNOWN, text);
      break;
    }
  }
}

void OrderEntryREST::test(web::http::Status status) {
  if (status != web::http::Status::FORBIDDEN) [[likely]] {
    return;
  }
  waf_limit_violation();
}

void OrderEntryREST::waf_limit_violation() {
  if (shared_.settings.rest.terminate_on_403) {
    log::fatal("WAF limit violation"sv);
  } else {
    log::warn("WAF limit violation"sv);
    (*connection_).suspend(shared_.settings.rest.back_off_delay);
  }
}

}  // namespace binance
}  // namespace roq
