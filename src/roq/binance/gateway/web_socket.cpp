/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/gateway/web_socket.hpp"

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

#include "roq/binance/protocol/json/encoder.hpp"
#include "roq/binance/protocol/json/map.hpp"
#include "roq/binance/protocol/json/utils.hpp"
#include "roq/binance/protocol/json/wsapi_type.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace gateway {

// === CONSTANTS ===

namespace {
auto const NAME = "ws"sv;

auto const SUPPORTS = Mask{
    SupportType::CREATE_ORDER,
    SupportType::MODIFY_ORDER,
    SupportType::CANCEL_ORDER,
    SupportType::ORDER_ACK,
    SupportType::ORDER,
    SupportType::TRADE,
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

auto create_connection(auto &handler, auto &settings, auto &context, auto &interface, auto &account) {
  auto uri = settings.ws_api_2.uri;
  auto config = web::socket::Client::Config{
      // connection
      .interface = interface,
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
  return web::socket::Client::create(handler, context, config, [headers = std::string{account.get_rest_headers_new()}]() { return headers; });
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

WebSocket::WebSocket(
    OrderEntry::Handler &handler, io::Context &context, uint16_t stream_id, Account &account, Shared &shared, std::string_view const &interface)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.name)},
      connection_{create_connection(*this, shared.settings, context, interface, account)},
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
          .account_status = create_metrics(shared.settings, name_, "account_status"sv),
          .open_orders_status = create_metrics(shared.settings, name_, "open_orders_status"sv),
          .my_trades = create_metrics(shared.settings, name_, "my_trades"sv),
          //
          .order_place = create_metrics(shared.settings, name_, "order_place"sv),
          .order_place_ack = create_metrics(shared.settings, name_, "order_place_ack"sv),
          .order_amend_keep_priority = create_metrics(shared.settings, name_, "order_amend_keep_priority"sv),
          .order_amend_keep_priority_ack = create_metrics(shared.settings, name_, "order_amend_keep_priority_ack"sv),
          .order_cancel = create_metrics(shared.settings, name_, "order_cancel"sv),
          .order_cancel_ack = create_metrics(shared.settings, name_, "order_cancel_ack"sv),
          .open_orders_cancel_all = create_metrics(shared.settings, name_, "open_orders_cancel_all"sv),
          .open_orders_cancel_all_ack = create_metrics(shared.settings, name_, "open_orders_cancel_all_ack"sv),
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
      account_{account}, shared_{shared}, request_id_{REQUEST_ID * stream_id},
      download_{shared.settings.rest.request_timeout, [this](auto state) { return download(state); }} {
  log::info<5>(R"(stream_id={}, account="{}")"sv, stream_id_, account_.name);
}

void WebSocket::operator()(Event<Start> const &) {
  (*connection_).start();
}

void WebSocket::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void WebSocket::operator()(Event<Timer> const &event) {
  auto &[message_info, timer] = event;
  (*connection_).refresh(timer.now);
}

void WebSocket::operator()(metrics::Writer &writer) const {
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
      .write(profile_.account_status, metrics::Type::PROFILE)
      .write(profile_.open_orders_status, metrics::Type::PROFILE)
      .write(profile_.my_trades, metrics::Type::PROFILE)
      //
      .write(profile_.order_place, metrics::Type::PROFILE)
      .write(profile_.order_place_ack, metrics::Type::PROFILE)
      .write(profile_.order_amend_keep_priority, metrics::Type::PROFILE)
      .write(profile_.order_amend_keep_priority_ack, metrics::Type::PROFILE)
      .write(profile_.order_cancel, metrics::Type::PROFILE)
      .write(profile_.order_cancel_ack, metrics::Type::PROFILE)
      .write(profile_.open_orders_cancel_all, metrics::Type::PROFILE)
      .write(profile_.open_orders_cancel_all_ack, metrics::Type::PROFILE)
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

uint16_t WebSocket::operator()(
    Event<CreateOrder> const &event, server::oms::Order const &order, server::oms::RefData const &ref_data, std::string_view const &request_id) {
  order_place(event, order, ref_data, request_id);
  return stream_id_;
}

uint16_t WebSocket::operator()(
    Event<ModifyOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  order_amend_keep_priority(event, order, ref_data, request_id, previous_request_id);
  return stream_id_;
}

uint16_t WebSocket::operator()(
    Event<CancelOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  order_cancel(event, order, ref_data, request_id, previous_request_id);
  return stream_id_;
}

uint16_t WebSocket::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  open_orders_cancel_all(event, request_id);
  return stream_id_;
}

// session-logon
// weight: 2

void WebSocket::session_logon() {
  auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
  auto request = protocol::json::WSAPIRequest{
      .sequence = ++request_id_,
      .type = protocol::json::WSAPIType::SESSION_LOGON,
      .user_id = {},
      .order_id = {},
      .version = {},
      .order_id_2 = {},
  };
  auto request_id = protocol::json::WSAPIRequest::encode(request_encode_buffer_, request);
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
  (*this)(ConnectionStatus::LOGIN_SENT, "session.logon"sv);
}

// user-data-stream-subscribe
// weight: 2

void WebSocket::user_data_stream_subscribe() {
  auto request = protocol::json::WSAPIRequest{
      .sequence = ++request_id_,
      .type = protocol::json::WSAPIType::USER_DATA_STREAM_SUBSCRIBE,
      .user_id = {},
      .order_id = {},
      .version = {},
      .order_id_2 = {},
  };
  auto request_id = protocol::json::WSAPIRequest::encode(request_encode_buffer_, request);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}",)"
      R"("method":"userDataStream.subscribe")"
      R"(}})"sv,
      request_id);
  (*connection_).send_text(message);
  (*this)(ConnectionStatus::DOWNLOADING, "userDataStream.subscribe"sv);
}

// account-status
// weight: 20

void WebSocket::account_status() {
  auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
  auto request = protocol::json::WSAPIRequest{
      .sequence = ++request_id_,
      .type = protocol::json::WSAPIType::ACCOUNT_STATUS,
      .user_id = {},
      .order_id = {},
      .version = {},
      .order_id_2 = {},
  };
  auto request_id = protocol::json::WSAPIRequest::encode(request_encode_buffer_, request);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}",)"
      R"("method":"account.status",)"
      R"("params":{{)"
      R"("timestamp":"{}")"
      R"(}})"
      R"(}})"sv,
      request_id,
      timestamp.count());
  (*connection_).send_text(message);
  (*this)(ConnectionStatus::DOWNLOADING, "account.status"sv);
}

// open-orders
// weight: 80 (symbol=none)

void WebSocket::open_orders_status() {
  auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
  auto request = protocol::json::WSAPIRequest{
      .sequence = ++request_id_,
      .type = protocol::json::WSAPIType::OPEN_ORDERS_STATUS,
      .user_id = {},
      .order_id = {},
      .version = {},
      .order_id_2 = {},
  };
  auto request_id = protocol::json::WSAPIRequest::encode(request_encode_buffer_, request);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}",)"
      R"("method":"openOrders.status",)"
      R"("params":{{)"
      R"("timestamp":"{}")"
      R"(}})"
      R"(}})"sv,
      request_id,
      timestamp.count());
  (*connection_).send_text(message);
  (*this)(ConnectionStatus::DOWNLOADING, "openOrders.status"sv);
}

// my-trades
// note! one request per symbol
// weight: 20 (without orderId)

void WebSocket::my_trades() {
  auto lookback = get_download_trades_lookback(shared_.settings, download_trades_is_first_);
  if (lookback.count() && !std::empty(shared_.settings.download.symbols)) {
    log::info<1>("Download trades: lookback={}, symbol={}"sv, lookback, shared_.settings.download.symbols);
    auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
    auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - lookback);
    auto limit = shared_.settings.download.trades_limit ? shared_.settings.download.trades_limit : DOWNLOAD_TRADES_LIMIT;
    for (auto &symbol : shared_.settings.download.symbols) {
      auto request = protocol::json::WSAPIRequest{
          .sequence = ++request_id_,
          .type = protocol::json::WSAPIType::MY_TRADES,
          .user_id = {},
          .order_id = {},
          .version = {},
          .order_id_2 = {},
      };
      auto request_id = protocol::json::WSAPIRequest::encode(request_encode_buffer_, request);
      auto message = fmt::format(
          R"({{)"
          R"("id":"{}",)"
          R"("method":"myTrades",)"
          R"("params":{{)"
          R"("timestamp":{},)"
          R"("limit":{},)"
          R"("startTime":{},)"
          R"("symbol":"{}")"
          R"(}})"
          R"(}})"sv,
          request_id,
          timestamp.count(),
          limit,
          start_time.count(),
          symbol);
      (*connection_).send_text(message);
      (*this)(ConnectionStatus::DOWNLOADING, "myTrades"sv);
    }
  } else {
    auto const STATE = State::MY_TRADES;
    download_.check_relaxed(STATE);
  }
}

// order-place
// weight: 1

void WebSocket::order_place(
    Event<CreateOrder> const &event, server::oms::Order const &order, server::oms::RefData const &ref_data, std::string_view const &request_id) {
  profile_.order_place([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, create_order] = event;
    open_orders_symbols_.emplace(create_order.symbol);
    auto &create_order_template = shared_.get_create_order_template(create_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
    auto params =
        protocol::json::Encoder::place_order_json(encode_buffer_, create_order, order, ref_data, request_id, create_order_template, recv_window, timestamp);
    auto request = protocol::json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = protocol::json::WSAPIType::ORDER_PLACE,
        .user_id = message_info.source,
        .order_id = create_order.order_id,
        .version = 1,
        .order_id_2 = {},
    };
    auto request_id_2 = protocol::json::WSAPIRequest::encode(request_encode_buffer_, request);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("method":"order.place",)"
        R"("params":{})"
        R"(}})"sv,
        request_id_2,
        params);
    log::debug("message={}"sv, message);
    (*connection_).send_text(message);
  });
}

// order-amend-keep-priority
// weight: 4

void WebSocket::order_amend_keep_priority(
    Event<ModifyOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  profile_.order_amend_keep_priority([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, modify_order] = event;
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
    auto params = protocol::json::Encoder::amend_order_keep_priority_json(
        encode_buffer_, modify_order, order, ref_data, request_id, previous_request_id, recv_window, timestamp);
    auto request = protocol::json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = protocol::json::WSAPIType::ORDER_AMEND_KEEP_PRIORITY,
        .user_id = message_info.source,
        .order_id = modify_order.order_id,
        .version = modify_order.version,
        .order_id_2 = {},
    };
    auto request_id_2 = protocol::json::WSAPIRequest::encode(request_encode_buffer_, request);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("method":"order.amend.keepPriority",)"
        R"("params":{})"
        R"(}})"sv,
        request_id_2,
        params);
    log::debug("message={}"sv, message);
    (*connection_).send_text(message);
  });
}

// order-cancel
// weight: 1

void WebSocket::order_cancel(
    Event<CancelOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  profile_.order_cancel([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, cancel_order] = event;
    auto &cancel_order_template = shared_.get_cancel_order_template(cancel_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
    auto params = protocol::json::Encoder::cancel_order_json(
        encode_buffer_, cancel_order, order, ref_data, request_id, previous_request_id, cancel_order_template, recv_window, timestamp);
    auto request = protocol::json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = protocol::json::WSAPIType::ORDER_CANCEL,
        .user_id = message_info.source,
        .order_id = cancel_order.order_id,
        .version = cancel_order.version,
        .order_id_2 = {},
    };
    auto request_id_2 = protocol::json::WSAPIRequest::encode(request_encode_buffer_, request);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("method":"order.cancel",)"
        R"("params":{})"
        R"(}})"sv,
        request_id_2,
        params);
    log::debug("message={}"sv, message);
    (*connection_).send_text(message);
  });
}

// open-orders-cancel-all
// note! one request per symbol
// weight: 1

void WebSocket::open_orders_cancel_all(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  profile_.open_orders_cancel_all([&]() {
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
    // XXX FIXME TODO roq-server provides this
    for (auto &symbol : open_orders_symbols_) {
      if (!std::empty(cancel_all_orders.symbol) && symbol != cancel_all_orders.symbol) {
        continue;
      }
      auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
      auto request = protocol::json::WSAPIRequest{
          .sequence = ++request_id_,
          .type = protocol::json::WSAPIType::OPEN_ORDERS_CANCEL_ALL,
          .user_id = message_info.source,
          .order_id = {},
          .version = {},
          .order_id_2 = {},
      };
      auto request_id_2 = protocol::json::WSAPIRequest::encode(request_encode_buffer_, request);  // XXX FIXME here we lose request_id
      auto message = fmt::format(
          R"({{)"
          R"("id":"{}",)"
          R"("method":"openOrders.cancelAll",)"
          R"("params":{{)"
          R"("symbol":"{}",)"
          R"("timestamp":"{}")"
          R"(}})"
          R"(}})"sv,
          request_id_2,
          symbol,
          timestamp.count());
      log::debug("message={}"sv, message);
      (*connection_).send_text(message);
      send_ack(symbol);
    }
  });
}

void WebSocket::operator()(web::socket::Client::Connected const &) {
  // wait for ready
}

void WebSocket::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
}

void WebSocket::operator()(web::socket::Client::Ready const &) {
  download_.begin();
}

void WebSocket::operator()(web::socket::Client::Close const &) {
}

void WebSocket::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = account_.name,
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void WebSocket::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void WebSocket::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected"sv);
}

void WebSocket::operator()(ConnectionStatus connection_status, std::string_view const &reason) {
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
  create_trace_and_dispatch(handler_, trace_info, stream_status);
}

uint32_t WebSocket::download(State state) {
  switch (state) {
    using enum State;
    case UNDEFINED:
      assert(false);
      break;
    case SESSION_LOGON:
      session_logon();
      return 1;
    case USER_DATA_STREAM_SUBSCRIBE:
      user_data_stream_subscribe();
      return 1;
    case ACCOUNT_STATUS:
      account_status();
      return 1;
    case OPEN_ORDERS_STATUS:
      open_orders_status();
      return 1;
    case MY_TRADES:
      my_trades();
      return 1;  // XXX FIXME TODO here we need a countdown
    case DONE:
      (*this)(ConnectionStatus::READY);
      assert(!ready_);
      ready_ = true;
      return 0;
  }
  assert(false);
  return 0;
}

void WebSocket::parse(std::string_view const &message) {
  profile_.parse([&]() {
    auto log_message = [&]() { log::warn(R"(*** PLEASE REPORT *** message="{}")"sv, message); };
    try {
      TraceInfo trace_info;
      if (!protocol::json::WSAPIParser::dispatch(*this, message, decode_buffer_, trace_info)) {
        log_message();
      }
    } catch (...) {
      log_message();
      utils::exceptions::Unhandled::terminate();
    }
  });
}

// protocol::json::WSAPIParser::Handler

void WebSocket::operator()(Trace<protocol::json::WSAPISessionLogon> const &event) {
  auto const STATE = State::SESSION_LOGON;
  profile_.session_logon([&]() {
    auto &[trace_info, wsapi_session_logon] = event;
    log::info<2>("wsapi_session_logon={}"sv, wsapi_session_logon);
    if (wsapi_session_logon.status == 200) {
      [[maybe_unused]] auto &result = wsapi_session_logon.result;
      download_.check_relaxed(STATE);
    } else {
      // XXX FIXME TODO review
      [[maybe_unused]] auto error = protocol::json::guess_error(wsapi_session_logon.error.code);
      log::error(R"(Unexpected: account="{}", error={})"sv, account_.name, wsapi_session_logon.error);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    }
    update_rate_limits(event);
  });
}

void WebSocket::operator()(Trace<protocol::json::WSAPIUserDataStreamSubscribe> const &event) {
  auto const STATE = State::USER_DATA_STREAM_SUBSCRIBE;
  profile_.user_data_stream_subscribe([&]() {
    auto &[trace_info, wsapi_user_data_stream_subscribe] = event;
    log::info<2>("wsapi_user_data_stream_subscribe={}"sv, wsapi_user_data_stream_subscribe);
    if (wsapi_user_data_stream_subscribe.status == 200) {
      download_.check_relaxed(STATE);
    } else {
      // XXX FIXME TODO review
      [[maybe_unused]] auto error = protocol::json::guess_error(wsapi_user_data_stream_subscribe.error.code);
      log::error(R"(Unexpected: account="{}", error={})"sv, account_.name, wsapi_user_data_stream_subscribe.error);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    }
    update_rate_limits(event);
  });
}

void WebSocket::operator()(Trace<protocol::json::WSAPIEventStreamTerminated> const &) {
  log::fatal("Unexpected"sv);
}

void WebSocket::operator()(Trace<protocol::json::WSAPIAccount> const &event) {
  auto const STATE = State::ACCOUNT_STATUS;
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
            .unrealized_pnl = NaN,
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
}

void WebSocket::operator()(Trace<protocol::json::WSAPIOpenOrders> const &event) {
  auto const STATE = State::OPEN_ORDERS_STATUS;
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
}

void WebSocket::operator()(Trace<protocol::json::WSAPITrades> const &event) {
  auto const STATE = State::MY_TRADES;
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
}

void WebSocket::operator()(Trace<protocol::json::WSAPIOrderPlace> const &event, protocol::json::WSAPIRequest const &request) {
  profile_.order_place_ack([&]() {
    auto &[trace_info, wsapi_order_place] = event;
    log::info<2>("wsapi_order_place={}, request={}"sv, wsapi_order_place, request);
    if (wsapi_order_place.status == 200) {
      auto &new_order = wsapi_order_place.result;
      auto external_order_id = fmt::format("{}"sv, new_order.order_id);  // alloc
      auto order_status = map(new_order.status).template get<OrderStatus>();
      // LIMIT_MAKER orders do not return any order state + we only end up here if we receive HTTP status OK
      if (order_status == OrderStatus{}) {
        order_status = OrderStatus::WORKING;
      }
      auto remaining_quantity = new_order.orig_qty - new_order.executed_qty;
      auto average_traded_price = utils::is_zero(new_order.executed_qty) ? NaN : (new_order.cummulative_quote_qty / new_order.executed_qty);
      auto last_traded_quantity = 0.0;  // note! could also use new_order.executed_qty
      auto tmp = 0.0;
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
          .version = request.version,
          .request_id = {},
          .external_order_id = {},
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
          .error = {},
          .text = {},
          .quantity = new_order.orig_qty,
          .price = new_order.price,
          .stop_price = NaN,
          .leverage = NaN,
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
      (*this)(event_2, request.user_id, request.order_id, order_update);
    } else {
      auto &error = wsapi_order_place.error;
      auto error_2 = protocol::json::guess_error(error.code);
      auto response = server::oms::Response{
          .request_type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::REJECTED,
          .error = error_2,
          .text = error.msg,
          .version = request.version,
          .request_id = {},
          .external_order_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, request.user_id, request.order_id);
    }
    update_rate_limits(event);
  });
}

void WebSocket::operator()(Trace<protocol::json::WSAPIOrderAmendKeepPriority> const &event, protocol::json::WSAPIRequest const &request) {
  profile_.order_amend_keep_priority_ack([&]() {
    auto &[trace_info, wsapi_order_amend_keep_priority] = event;
    log::info<2>("wsapi_order_amend_keep_priority={}, request={}"sv, wsapi_order_amend_keep_priority, request);
    if (wsapi_order_amend_keep_priority.status == 200) {
      auto &order_amend_keep_priority = wsapi_order_amend_keep_priority.result;
      auto &amended_order = order_amend_keep_priority.amended_order;
      auto external_order_id = fmt::format("{}"sv, amended_order.order_id);  // alloc
      auto response = server::oms::Response{
          .request_type = RequestType::MODIFY_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = request.version,
          .request_id = {},
          .external_order_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      auto order_update = server::oms::OrderUpdate{
          .account = account_.name,
          .exchange = shared_.settings.exchange,
          .symbol = amended_order.symbol,
          .side = map(amended_order.side),
          .position_effect = {},
          .margin_mode = {},
          .max_show_quantity = NaN,
          .order_type = map(amended_order.type),
          .time_in_force = map(amended_order.time_in_force),
          .execution_instructions = {},
          .create_time_utc = {},
          .update_time_utc = order_amend_keep_priority.transact_time,
          .external_account = {},
          .external_order_id = external_order_id,
          .client_order_id = {},
          .order_status = map(amended_order.status),
          .error = {},
          .text = {},
          .quantity = amended_order.qty,
          .price = amended_order.price,
          .stop_price = NaN,
          .leverage = NaN,
          .remaining_quantity = NaN,
          .traded_quantity = amended_order.executed_qty,  // cumulative_qty ???
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
      (*this)(event_2, request.user_id, request.order_id, order_update);
    } else {
      auto &error = wsapi_order_amend_keep_priority.error;
      auto error_2 = protocol::json::guess_error(error.code);
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::REJECTED,
          .error = error_2,
          .text = error.msg,
          .version = request.version,
          .request_id = {},
          .external_order_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, request.user_id, request.order_id);
    }
    update_rate_limits(event);
  });
}

void WebSocket::operator()(Trace<protocol::json::WSAPICancelOrder> const &event, protocol::json::WSAPIRequest const &request) {
  profile_.order_cancel_ack([&]() {
    auto &[trace_info, wsapi_cancel_order] = event;
    log::info<2>("wsapi_cancel_order={}, request={}"sv, wsapi_cancel_order, request);
    if (wsapi_cancel_order.status == 200) {
      auto &cancel_order = wsapi_cancel_order.result;
      auto external_order_id = fmt::format("{}"sv, cancel_order.order_id);  // alloc
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = request.version,
          .request_id = {},
          .external_order_id = {},
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
          .update_time_utc = cancel_order.transact_time,
          .external_account = {},
          .external_order_id = external_order_id,
          .client_order_id = {},
          .order_status = map(cancel_order.status),
          .error = {},
          .text = {},
          .quantity = cancel_order.orig_qty,
          .price = cancel_order.price,
          .stop_price = NaN,
          .leverage = NaN,
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
      (*this)(event_2, request.user_id, request.order_id, order_update);
    } else {
      auto &error = wsapi_cancel_order.error;
      auto error_2 = protocol::json::guess_error(error.code);
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::REJECTED,
          .error = error_2,
          .text = error.msg,
          .version = request.version,
          .request_id = {},
          .external_order_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, request.user_id, request.order_id);
    }
    update_rate_limits(event);
  });
}

void WebSocket::operator()(Trace<protocol::json::WSAPICancelOpenOrders> const &event, protocol::json::WSAPIRequest const &request) {
  profile_.open_orders_cancel_all_ack([&]() {
    auto &[trace_info, wsapi_cancel_open_orders] = event;
    log::info<2>("wsapi_cancel_open_orders={}, request={}"sv, wsapi_cancel_open_orders, request);
    if (wsapi_cancel_open_orders.status == 200) {
      auto &cancel_all_open_orders = wsapi_cancel_open_orders.result;
      for (auto &item : cancel_all_open_orders) {
        log::info<2>("item={}"sv, item);
        if (std::empty(item.client_order_id)) {
          continue;
        }
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
            .client_order_id = {},
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
            .update_type = UpdateType::INCREMENTAL,
            .sending_time_utc = {},
        };
        shared_.update_order(item.client_order_id, stream_id_, trace_info, order_update, []([[maybe_unused]] auto &order) {});
      }
    } else {
      log::error(R"(Unexpected: account="{}", error={})"sv, account_.name, wsapi_cancel_open_orders.error);
    }
    update_rate_limits(event);
  });
}

void WebSocket::operator()(Trace<protocol::json::WSAPIOutboundAccountPosition> const &event) {
  profile_.outbound_account_position([&]() {
    auto &[trace_info, wsapi_outbound_account_position] = event;
    log::info<2>("wsapi_outbound_account_position={}"sv, wsapi_outbound_account_position);
    Trace event_2{trace_info, wsapi_outbound_account_position.event};
    (*this)(event_2);
  });
}

void WebSocket::operator()(Trace<protocol::json::WSAPIBalanceUpdate> const &event) {
  profile_.balance_update([&]() {
    auto &[trace_info, wsapi_balance_update] = event;
    log::info<2>("wsapi_balance_update={}"sv, wsapi_balance_update);
    Trace event_2{trace_info, wsapi_balance_update.event};
    (*this)(event_2);
  });
}

void WebSocket::operator()(Trace<protocol::json::WSAPIExecutionReport> const &event) {
  profile_.execution_report([&]() {
    auto &[trace_info, wsapi_execution_report] = event;
    log::info<2>("wsapi_execution_report={}"sv, wsapi_execution_report);
    Trace event_2{trace_info, wsapi_execution_report.event};
    (*this)(event_2);
  });
}

// helpers

void WebSocket::operator()(Trace<protocol::json::OutboundAccountPositionData> const &event) {
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
        .unrealized_pnl = NaN,
        .external_account = {},
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = outbound_account_position.time_of_last_account_update,
        .sending_time_utc = outbound_account_position.event_time,
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

void WebSocket::operator()(Trace<protocol::json::BalanceUpdateData> const &) {
  // note! contains delta (changes) -- we're not going to use here
}

void WebSocket::operator()(Trace<protocol::json::ExecutionReportData> const &event) {
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
  if (execution_report.current_execution_type != protocol::json::ExecutionType::TRADE) {
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

void WebSocket::update_rate_limits(auto &event) {
  auto &[trace_info, message] = event;
  shared_.rate_limits.clear();
  for (auto &item : message.rate_limits) {
    auto type = [&]() -> RateLimitType {
      switch (item.rate_limit_type) {
        using enum protocol::json::RateLimitType::type_t;
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
        using enum protocol::json::Interval::type_t;
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
      case CREATE_AND_MODIFY_ORDER:
      case MODIFY_ORDER:
      case CANCEL_ORDER:
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
    Trace event_2{trace_info, rate_limits_update};
    handler_(event_2);
  }
  shared_.rate_limits.clear();
}

template <typename... Args>
void WebSocket::operator()(Trace<server::oms::Response> const &event, uint8_t user_id, uint64_t order_id, Args &&...args) {
  auto &[trace_info, response] = event;
  if (shared_.update_order(user_id, order_id, stream_id_, trace_info, response, std::forward<Args>(args)..., []([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
  }
}

void WebSocket::operator()(Trace<server::oms::OrderUpdate> const &event, std::string_view const &client_order_id) {
  auto &[trace_info, order_update] = event;
  if (shared_.update_order(client_order_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

}  // namespace gateway
}  // namespace binance
}  // namespace roq
