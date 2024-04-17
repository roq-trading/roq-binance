/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/order_entry_ws.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "roq/mask.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/tools/exception.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/binance/json/utils.hpp"
#include "roq/binance/json/wsapi_type.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const NAME = "ex"sv;

auto const SUPPORTS = Mask{
    SupportType::CREATE_ORDER,
    SupportType::CANCEL_ORDER,
    SupportType::ORDER_ACK,
    SupportType::ORDER,
    SupportType::FUNDS,
};

auto const REQUEST_ID = uint32_t{1000000};
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto &account) {
  return fmt::format("{}:{}:{}"sv, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &settings, auto &context, auto &interface) {
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
      .query = {},
      .user_agent = ROQ_PACKAGE_NAME,
      .request_timeout = {},
      .ping_frequency = settings.ws_api_2.ping_freq,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::socket::Client::create(handler, context, config, []() -> std::string { return {}; });
}

struct create_metrics final : public core::metrics::Factory {
  create_metrics(auto &settings, auto const &group, auto const &function)
      : core::metrics::Factory(settings.app.name, group, function) {}
  create_metrics(auto &settings, auto const &group, auto const &function, auto const &params)
      : core::metrics::Factory(settings.app.name, group, function, params) {}
};

auto get_download_trades_lookback(auto const &settings, auto download_trades_is_first) {
  if (download_trades_is_first) {
    if (settings.download.trades_lookback_on_restart.count())
      return settings.download.trades_lookback_on_restart;
  }
  return settings.download.trades_lookback;
}
}  // namespace

// === IMPLEMENTATION ===

OrderEntryWS::OrderEntryWS(
    OrderEntry::Handler &handler,
    io::Context &context,
    uint16_t stream_id,
    Account &account,
    Shared &shared,
    Request &request,
    bool master,
    std::string_view const &interface)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.name)}, master_{master},
      connection_{create_connection(*this, shared.settings, context, interface)},
      decode_buffer_(shared.settings.misc.decode_buffer_size),
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(shared.settings, name_, "parse"sv),
          .error = create_metrics(shared.settings, name_, "error"sv),
          .user_data_stream_start = create_metrics(shared.settings, name_, "user_data_stream_start"sv),
          .user_data_stream_start_ack = create_metrics(shared.settings, name_, "user_data_stream_start_ack"sv),
          .user_data_stream_ping = create_metrics(shared.settings, name_, "user_data_stream_ping"sv),
          .user_data_stream_ping_ack = create_metrics(shared.settings, name_, "user_data_stream_ping_ack"sv),
          .account_status = create_metrics(shared.settings, name_, "account_status"sv),
          .account_status_ack = create_metrics(shared.settings, name_, "account_status_ack"sv),
          .open_orders_status = create_metrics(shared.settings, name_, "open_orders_status"sv),
          .open_orders_status_ack = create_metrics(shared.settings, name_, "open_orders_status_ack"sv),
          .my_trades = create_metrics(shared.settings, name_, "my_trades"sv),
          .my_trades_ack = create_metrics(shared.settings, name_, "my_trades_ack"sv),
          .open_orders_cancel_all = create_metrics(shared.settings, name_, "open_orders_cancel_all"sv),
          .open_orders_cancel_all_ack = create_metrics(shared.settings, name_, "open_orders_cancel_all_ack"sv),
          .order_place = create_metrics(shared.settings, name_, "order_place"sv),
          .order_place_ack = create_metrics(shared.settings, name_, "order_place_ack"sv),
          .order_cancel = create_metrics(shared.settings, name_, "order_cancel"sv),
          .order_cancel_ack = create_metrics(shared.settings, name_, "order_cancel_ack"sv),
          .order_cancel_replace = create_metrics(shared.settings, name_, "order_cancel_replace"sv),
          .order_cancel_replace_ack = create_metrics(shared.settings, name_, "order_cancel_replace_ack"sv),
          .order_cancel_replace_error = create_metrics(shared.settings, name_, "order_cancel_replace_error"sv),
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
      account_{account}, shared_{shared}, request_{request}, request_id_{REQUEST_ID * stream_id} {
  log::info<5>(R"(stream_id={}, account="{}")"sv, stream_id_, account_.name);
}

void OrderEntryWS::operator()(Event<Start> const &) {
  (*connection_).start();
}

void OrderEntryWS::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void OrderEntryWS::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  user_data_stream_ping(now);
  if (master_ && ready() && !downloading()) {
    if (!downloading() && request_.respond_account < request_.request_account) {
      log::info("Download account..."sv);
      account_status();
      download_account_ = true;
    }
    if (!downloading() && request_.respond_orders < request_.request_orders) {
      log::info("Download orders..."sv);
      open_orders_status();
      download_orders_ = true;
    }
    if (!downloading() && request_.respond_trades < request_.request_trades) {
      log::info("Download trades..."sv);
      my_trades();
      download_trades_ = true;
    }
  }
}

void OrderEntryWS::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.parse, metrics::Type::PROFILE)
      .write(profile_.error, metrics::Type::PROFILE)
      .write(profile_.user_data_stream_start, metrics::Type::PROFILE)
      .write(profile_.user_data_stream_start_ack, metrics::Type::PROFILE)
      .write(profile_.user_data_stream_ping, metrics::Type::PROFILE)
      .write(profile_.user_data_stream_ping_ack, metrics::Type::PROFILE)
      .write(profile_.account_status, metrics::Type::PROFILE)
      .write(profile_.account_status_ack, metrics::Type::PROFILE)
      .write(profile_.open_orders_status, metrics::Type::PROFILE)
      .write(profile_.open_orders_status_ack, metrics::Type::PROFILE)
      .write(profile_.open_orders_cancel_all, metrics::Type::PROFILE)
      .write(profile_.open_orders_cancel_all_ack, metrics::Type::PROFILE)
      .write(profile_.order_place, metrics::Type::PROFILE)
      .write(profile_.order_place_ack, metrics::Type::PROFILE)
      .write(profile_.order_cancel, metrics::Type::PROFILE)
      .write(profile_.order_cancel_ack, metrics::Type::PROFILE)
      .write(profile_.order_cancel_replace, metrics::Type::PROFILE)
      .write(profile_.order_cancel_replace_ack, metrics::Type::PROFILE)
      .write(profile_.order_cancel_replace_error, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      .write(latency_.heartbeat, metrics::Type::LATENCY)
      // rate limiter
      .write(rate_limiter_.request_weight_1m, metrics::Type::RATE_LIMITER)
      .write(rate_limiter_.create_order_10s, metrics::Type::RATE_LIMITER)
      .write(rate_limiter_.create_order_1d, metrics::Type::RATE_LIMITER);
}

void OrderEntryWS::operator()(Event<Disconnected> const &event) {
  auto user_id = event.message_info.source;
  account_.cancel_order_request_buffer_[user_id].reset();
}

uint16_t OrderEntryWS::operator()(
    Event<CreateOrder> const &event, server::oms::Order const &order, std::string_view const &request_id) {
  auto &message_info = event.message_info;
  auto &tmp = account_.cancel_order_request_buffer_[message_info.source];
  if (!tmp) {
    order_place(event, order, request_id);
  } else {
    // cancel + replace
    typename std::remove_cvref<decltype(tmp)>::type tmp2;
    tmp.swap(tmp2);
    // XXX do we get error on cancel if we can't send?
    order_cancel_replace(*tmp2, event, order, request_id);
  }
  return stream_id_;
}

uint16_t OrderEntryWS::operator()(
    Event<ModifyOrder> const &,
    server::oms::Order const &,
    [[maybe_unused]] std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id) {
  throw server::oms::NotSupported{"not supported"sv};
}

uint16_t OrderEntryWS::operator()(
    Event<CancelOrder> const &event,
    server::oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto &[message_info, cancel_order] = event;
  auto &tmp = account_.cancel_order_request_buffer_[message_info.source];
  if (tmp)
    throw server::oms::NotSupported{"not supported"sv};
  if (message_info.is_last) {
    order_cancel(event, order, request_id, previous_request_id);
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

uint16_t OrderEntryWS::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  open_orders_cancel_all(event, request_id);
  return stream_id_;
}

// listen-key

void OrderEntryWS::user_data_stream_start() {
  profile_.user_data_stream_start([&]() {
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::LISTEN_KEY_CREATE,
        .user_id = {},
        .order_id = {},
        .version = {},
        .order_id_2 = {},
    };
    auto request_id = json::WSAPIRequest::encode(request_encode_buffer_, request);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("method":"userDataStream.start",)"
        R"("params":{{)"
        R"("apiKey":"{}")"
        R"(}})"
        R"(}})"sv,
        request_id,
        account_.get_key());
    (*connection_).send_text(message);
  });
}

void OrderEntryWS::user_data_stream_ping(std::chrono::nanoseconds now) {
  profile_.user_data_stream_ping([&]() {
    if (!ready())
      return;
    if (std::empty(listen_key_))
      return;
    if (listen_key_refresh_ == listen_key_refresh_.zero() || now < listen_key_refresh_)
      return;
    log::info<1>("Refreshing listen key..."sv);
    listen_key_refresh_ = now + shared_.settings.rest.listen_key_refresh;
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::LISTEN_KEY_PING,
        .user_id = {},
        .order_id = {},
        .version = {},
        .order_id_2 = {},
    };
    auto request_id = json::WSAPIRequest::encode(request_encode_buffer_, request);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("method":"userDataStream.ping",)"
        R"("params":{{)"
        R"("apiKey":"{}",)"
        R"("listenKey":"{}")"
        R"(}})"
        R"(}})"sv,
        request_id,
        account_.get_key(),
        listen_key_);
    (*connection_).send_text(message);
  });
}

// account

void OrderEntryWS::account_status() {
  profile_.account_status([&]() {
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto message_for_signature = fmt::format("apiKey={}&timestamp={}"sv, account_.get_key(), now.count());
    auto signature = account_.create_ws_api_signature(message_for_signature);
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::ACCOUNT_STATUS,
        .user_id = {},
        .order_id = {},
        .version = {},
        .order_id_2 = {},
    };
    auto request_id = json::WSAPIRequest::encode(request_encode_buffer_, request);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("method":"account.status",)"
        R"("params":{{)"
        R"("apiKey":"{}",)"
        R"("timestamp":"{}",)"
        R"("signature":"{}")"
        R"(}})"
        R"(}})"sv,
        request_id,
        account_.get_key(),
        now.count(),
        signature);
    (*connection_).send_text(message);
  });
}

// open-orders

void OrderEntryWS::open_orders_status() {
  profile_.open_orders_status([&]() {
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto message_for_signature = fmt::format("apiKey={}&timestamp={}"sv, account_.get_key(), now.count());
    auto signature = account_.create_ws_api_signature(message_for_signature);
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::OPEN_ORDERS_STATUS,
        .user_id = {},
        .order_id = {},
        .version = {},
        .order_id_2 = {},
    };
    auto request_id = json::WSAPIRequest::encode(request_encode_buffer_, request);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("method":"openOrders.status",)"
        R"("params":{{)"
        R"("apiKey":"{}",)"
        R"("timestamp":"{}",)"
        R"("signature":"{}")"
        R"(}})"
        R"(}})"sv,
        request_id,
        account_.get_key(),
        now.count(),
        signature);
    (*connection_).send_text(message);
  });
}

// my-trades

void OrderEntryWS::my_trades() {
  profile_.my_trades([&]() {
    auto &symbols = shared_.settings.download.symbols;
    for (auto &symbol : symbols) {
      auto now = clock::get_realtime<std::chrono::milliseconds>();
      auto lookback = get_download_trades_lookback(shared_.settings, download_trades_is_first_);
      log::info<1>("Download trades: lookback={}"sv, lookback);
      auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - lookback);
      // note! remember to sort
      auto message_for_signature = fmt::format(
          "apiKey={}&"
          "limit={}&"
          "startTime={}&"
          "symbol={}&"
          "timestamp={}"sv,
          account_.get_key(),
          shared_.settings.download.trades_limit,
          start_time.count(),
          symbol,
          now.count());
      auto signature = account_.create_ws_api_signature(message_for_signature);
      auto request = json::WSAPIRequest{
          .sequence = ++request_id_,
          .type = json::WSAPIType::MY_TRADES,
          .user_id = {},
          .order_id = {},
          .version = {},
          .order_id_2 = {},
      };
      auto request_id = json::WSAPIRequest::encode(request_encode_buffer_, request);
      auto message = fmt::format(
          R"({{)"
          R"("id":"{}",)"
          R"("method":"myTrades",)"
          R"("params":{{)"
          R"("apiKey":"{}",)"
          R"("limit":{},)"
          R"("startTime":{},)"
          R"("symbol":"{}",)"
          R"("timestamp":{},)"
          R"("signature":"{}")"
          R"(}})"
          R"(}})"sv,
          request_id,
          account_.get_key(),
          shared_.settings.download.trades_limit,
          start_time.count(),
          symbol,
          now.count(),
          signature);
      (*connection_).send_text(message);
    }
  });
}

// open-orders-cancel-all

void OrderEntryWS::open_orders_cancel_all(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  profile_.open_orders_cancel_all([&]() {
    if (!ready()) [[unlikely]]
      throw server::oms::NotReady{"not ready"sv};
    auto &message_info = event.message_info;
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
    for (auto &symbol : open_orders_symbols_) {
      if (!std::empty(cancel_all_orders.symbol) && symbol != cancel_all_orders.symbol)
        continue;
      auto now = clock::get_realtime<std::chrono::milliseconds>();
      auto message_for_signature =
          fmt::format("apiKey={}&symbol={}&timestamp={}"sv, account_.get_key(), symbol, now.count());
      auto signature = account_.create_ws_api_signature(message_for_signature);
      auto request = json::WSAPIRequest{
          .sequence = ++request_id_,
          .type = json::WSAPIType::OPEN_ORDERS_CANCEL_ALL,
          .user_id = message_info.source,
          .order_id = {},
          .version = {},
          .order_id_2 = {},
      };
      auto request_id_2 =
          json::WSAPIRequest::encode(request_encode_buffer_, request);  // XXX FIXME here we lose request_id
      auto message = fmt::format(
          R"({{)"
          R"("id":"{}",)"
          R"("method":"openOrders.cancelAll",)"
          R"("params":{{)"
          R"("apiKey":"{}",)"
          R"("symbol":"{}",)"
          R"("timestamp":"{}",)"
          R"("signature":"{}")"
          R"(}})"
          R"(}})"sv,
          request_id_2,
          account_.get_key(),
          symbol,
          now.count(),
          signature);
      (*connection_).send_text(message);
      send_ack(symbol);
    }
  });
}

// order-place

void OrderEntryWS::order_place(
    Event<CreateOrder> const &event, server::oms::Order const &order, std::string_view const &request_id) {
  profile_.order_place([&]() {
    if (!ready())
      throw server::oms::NotReady{"not ready"sv};
    auto &[message_info, create_order] = event;
    open_orders_symbols_.emplace(create_order.symbol);
    auto &create_order_template = shared_.get_create_order_template(create_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto message_for_signature = json::new_order_ws_url(
        encode_buffer_, create_order, order, request_id, create_order_template, recv_window, account_.get_key(), now);
    auto signature = account_.create_ws_api_signature(message_for_signature);
    auto params = json::new_order_ws_json(
        encode_buffer_,
        create_order,
        order,
        request_id,
        create_order_template,
        recv_window,
        account_.get_key(),
        now,
        signature);
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::ORDER_PLACE,
        .user_id = message_info.source,
        .order_id = create_order.order_id,
        .version = 1,
        .order_id_2 = {},
    };
    auto request_id_2 = json::WSAPIRequest::encode(request_encode_buffer_, request);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("method":"order.place",)"
        R"("params":{})"
        R"(}})"sv,
        request_id_2,
        params);
    (*connection_).send_text(message);
  });
}

// order-cancel

void OrderEntryWS::order_cancel(
    Event<CancelOrder> const &event,
    server::oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  profile_.order_cancel([&]() {
    if (!ready())
      throw server::oms::NotReady{"not ready"sv};
    auto &[message_info, cancel_order] = event;
    auto &cancel_order_template = shared_.get_cancel_order_template(cancel_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto message_for_signature = json::cancel_order_ws_url(
        encode_buffer_,
        cancel_order,
        order,
        request_id,
        previous_request_id,
        cancel_order_template,
        recv_window,
        account_.get_key(),
        now);
    auto signature = account_.create_ws_api_signature(message_for_signature);
    auto params = json::cancel_order_ws_json(
        encode_buffer_,
        cancel_order,
        order,
        request_id,
        previous_request_id,
        cancel_order_template,
        recv_window,
        account_.get_key(),
        now,
        signature);
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::ORDER_CANCEL,
        .user_id = message_info.source,
        .order_id = cancel_order.order_id,
        .version = cancel_order.version,
        .order_id_2 = {},
    };
    auto request_id_2 = json::WSAPIRequest::encode(request_encode_buffer_, request);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("method":"order.cancel",)"
        R"("params":{})"
        R"(}})"sv,
        request_id_2,
        params);
    (*connection_).send_text(message);
  });
}

// order-cancel-replace

void OrderEntryWS::order_cancel_replace(
    server::cache::CancelOrderRequest const &cancel_order_request,
    Event<CreateOrder> const &event,
    server::oms::Order const &order,
    std::string_view const &request_id) {
  profile_.order_cancel_replace([&]() {
    if (!ready())
      throw server::oms::NotReady{"not ready"sv};
    if (shared_.find_order(
            event.message_info.source, cancel_order_request.cancel_order.order_id, [&](auto &cancel_order) {
              auto &[message_info, create_order] = event;
              auto &cancel_order_template =
                  shared_.get_cancel_order_template(cancel_order_request.cancel_order.request_template);
              auto &create_order_template = shared_.get_create_order_template(create_order.request_template);
              auto now = clock::get_realtime<std::chrono::milliseconds>();
              auto message_for_signature = json::cancel_replace_order_ws_url(
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
                  shared_.settings.oms.cancel_replace_stop_on_failure,
                  account_.get_key(),
                  now);
              auto signature = account_.create_ws_api_signature(message_for_signature);
              auto params = json::cancel_replace_order_ws_json(
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
                  shared_.settings.oms.cancel_replace_stop_on_failure,
                  account_.get_key(),
                  now,
                  signature);
              auto request = json::WSAPIRequest{
                  .sequence = ++request_id_,
                  .type = json::WSAPIType::ORDER_CANCEL_REPLACE,
                  .user_id = message_info.source,
                  .order_id = cancel_order_request.cancel_order.order_id,
                  .version = cancel_order_request.cancel_order.version,
                  .order_id_2 = create_order.order_id,
              };
              auto request_id_2 = json::WSAPIRequest::encode(request_encode_buffer_, request);
              auto message = fmt::format(
                  R"({{)"
                  R"("id":"{}",)"
                  R"("method":"order.cancelReplace",)"
                  R"("params":{})"
                  R"(}})"sv,
                  request_id_2,
                  params);
              (*connection_).send_text(message);
            })) {
    } else {
      throw server::oms::Rejected{Origin::GATEWAY, Error::CONDITIONAL_REQUEST_HAS_FAILED, "internal error"sv};
    }
  });
}

void OrderEntryWS::operator()(web::socket::Client::Connected const &) {
}

void OrderEntryWS::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  // XXX FIXME also reset the download_* latches?
}

void OrderEntryWS::operator()(web::socket::Client::Ready const &) {
  if (master_) {
    user_data_stream_start();
    (*this)(ConnectionStatus::LOGIN_SENT);
  } else {
    (*this)(ConnectionStatus::READY);
  }
}

void OrderEntryWS::operator()(web::socket::Client::Close const &) {
}

void OrderEntryWS::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = account_.name,
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntryWS::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void OrderEntryWS::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntryWS::operator()(ConnectionStatus status) {
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

void OrderEntryWS::parse(std::string_view const &message) {
  profile_.parse([&]() {
    auto log_message = [&]() { log::warn(R"(message="{}")"sv, message); };
    try {
      TraceInfo trace_info;
      if (!json::WSAPIParser2::dispatch(*this, message, decode_buffer_, trace_info))
        log_message();
    } catch (...) {
      log_message();
      core::tools::UnhandledException::terminate();
    }
  });
}

// json::WSAPIParser2::Handler

void OrderEntryWS::operator()(Trace<json::WSAPIListenKey> const &event, json::WSAPIRequest const &request) {
  profile_.user_data_stream_start_ack([&]() {
    auto &[trace_info, message] = event;
    log::info<2>("message={}, request={}"sv, message, request);
    switch (message.status) {
      case 200: {
        auto &listen_key = message.result;
        listen_key_ = listen_key.listen_key;
        log::info<1>(R"(Listen key has been acquired (value="{}"))"sv, listen_key_);
        auto listen_key_update = ListenKeyUpdate{
            .account = account_.name,
            .margin_mode = {},
            .listen_key = listen_key_,
        };
        create_trace_and_dispatch(handler_, trace_info, listen_key_update);
        (*this)(ConnectionStatus::READY);
        auto now = clock::get_system();
        listen_key_refresh_ = now + shared_.settings.rest.listen_key_refresh;
        break;
      }
      default:
        switch (message.error.code) {
          case -2015:  // invalid key
            log::error("Unexpected: error={}"sv, message.error);
        }
    }
    update_rate_limits(event);
  });
}

void OrderEntryWS::operator()(Trace<json::WSAPIAccount> const &event, json::WSAPIRequest const &request) {
  profile_.user_data_stream_ping_ack([&]() {
    auto &[trace_info, message] = event;
    log::info<2>("message={}, request={}"sv, message, request);
    switch (message.status) {
      case 200: {
        auto &account = message.result;
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
        break;
      }
      default:
        log::error("Unexpected: error={}"sv, message.error);
    }
    // completion
    request_.respond_account = clock::get_system();
    download_account_ = false;
    update_rate_limits(event);
  });
}

void OrderEntryWS::operator()(Trace<json::WSAPIOpenOrders> const &event, json::WSAPIRequest const &request) {
  profile_.open_orders_status_ack([&]() {
    auto &[trace_info, message] = event;
    log::info<2>("message={}, request={}"sv, message, request);
    switch (message.status) {
      case 200: {
        auto &open_orders = message.result;
        for (auto &item : open_orders) {
          log::info<2>("item={}"sv, item);
          if (std::empty(item.client_order_id))
            continue;
          open_orders_symbols_.emplace(item.symbol);
          auto side = json::map(item.side);
          auto order_type = json::map(item.type);
          auto time_in_force = json::map(item.time_in_force);
          auto external_order_id = fmt::format("{}"sv, item.order_id);  // alloc
          auto order_status = json::map(item.status);
          auto order_update = server::oms::OrderUpdate{
              .account = account_.name,
              .exchange = shared_.settings.exchange,
              .symbol = item.symbol,
              .side = side,
              .position_effect = {},
              .margin_mode = {},
              .max_show_quantity = NaN,
              .order_type = order_type,
              .time_in_force = time_in_force,
              .execution_instructions = {},
              .create_time_utc = item.time,
              .update_time_utc = item.update_time,
              .external_account = {},
              .external_order_id = external_order_id,
              .client_order_id = item.client_order_id,
              .order_status = order_status,
              .quantity = item.orig_qty,
              .price = item.price,
              .stop_price = item.stop_price,
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
        break;
      }
      default:
        log::error("Unexpected: error={}"sv, message.error);
    }
    // completion
    request_.respond_orders = clock::get_system();
    download_orders_ = false;
    update_rate_limits(event);
  });
}

void OrderEntryWS::operator()(Trace<json::WSAPITrades> const &event, json::WSAPIRequest const &request) {
  profile_.my_trades_ack([&]() {
    auto &[trace_info, message] = event;
    log::info<2>("message={}, request={}"sv, message, request);
    switch (message.status) {
      case 200: {
        download_trades_is_first_ = false;  // after first successful
        auto &trades = message.result;
        for (auto &item : trades) {
          log::info<2>("item={}"sv, item);
          auto liquidity = item.is_maker ? Liquidity::MAKER : Liquidity::TAKER;
          auto fill = Fill{
              .external_trade_id = {},
              .quantity = item.qty,  // XXX FIXME quote_qty ???
              .price = item.price,
              .liquidity = liquidity,
          };
          auto side = item.is_buyer ? Side::BUY : Side::SELL;
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
        break;
      }
      default:
        log::error("Unexpected: error={}"sv, message.error);
    }
    // completion
    request_.respond_trades = clock::get_system();
    download_trades_ = false;
    update_rate_limits(event);
  });
}

void OrderEntryWS::operator()(Trace<json::WSAPICancelOpenOrders> const &event, json::WSAPIRequest const &request) {
  profile_.open_orders_cancel_all_ack([&]() {
    auto &[trace_info, message] = event;
    log::info<2>("message={}, request={}"sv, message, request);
    switch (message.status) {
      case 200: {
        auto &cancel_all_open_orders = message.result;
        for (auto &item : cancel_all_open_orders) {
          log::info<2>("item={}"sv, item);
          if (std::empty(item.client_order_id))
            continue;
          auto side = json::map(item.side);
          auto order_type = json::map(item.type);
          auto time_in_force = json::map(item.time_in_force);
          auto external_order_id = fmt::format("{}"sv, item.order_id);  // alloc
          auto order_status = json::map(item.status);
          auto order_update = server::oms::OrderUpdate{
              .account = account_.name,
              .exchange = shared_.settings.exchange,
              .symbol = item.symbol,
              .side = side,
              .position_effect = {},
              .margin_mode = {},
              .max_show_quantity = NaN,
              .order_type = order_type,
              .time_in_force = time_in_force,
              .execution_instructions = {},
              .create_time_utc = item.time,
              .update_time_utc = item.update_time,
              .external_account = {},
              .external_order_id = external_order_id,
              .client_order_id = {},
              .order_status = order_status,
              .quantity = item.orig_qty,
              .price = item.price,
              .stop_price = item.stop_price,
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
          shared_.update_order(
              item.client_order_id, stream_id_, trace_info, order_update, []([[maybe_unused]] auto &order) {});
        }
        break;
      }
      default:
        log::error("Unexpected: error={}"sv, message.error);
    }
    update_rate_limits(event);
  });
}

void OrderEntryWS::operator()(Trace<json::WSAPIOrderPlace> const &event, json::WSAPIRequest const &request) {
  profile_.order_place_ack([&]() {
    auto &[trace_info, message] = event;
    log::info<2>("message={}, request={}"sv, message, request);
    switch (message.status) {
      case 200: {
        auto &new_order = message.result;
        auto side = json::map(new_order.side);
        auto order_type = json::map(new_order.type);
        auto time_in_force = json::map(new_order.time_in_force);
        auto external_order_id = fmt::format("{}"sv, new_order.order_id);  // alloc
        auto order_status = json::map(new_order.status);
        // LIMIT_MAKER orders do not return any order state + we only end up here if we receive HTTP status OK
        if (order_status == OrderStatus{})
          order_status = OrderStatus::WORKING;
        auto remaining_quantity = new_order.orig_qty - new_order.executed_qty;
        auto average_traded_price =
            utils::is_zero(new_order.executed_qty) ? NaN : (new_order.cummulative_quote_qty / new_order.executed_qty);
        auto last_traded_quantity = double{0.0};  // note! could also use new_order.executed_qty
        auto tmp = double{0.0};
        for (auto &item : new_order.fills) {
          last_traded_quantity += item.qty;
          tmp += item.price * item.qty;
        }
        auto last_traded_price = NaN;  // note! could also use average_traded_price
        if (utils::is_greater(last_traded_quantity, 0.0))
          last_traded_price = tmp / last_traded_quantity;
        auto response = server::oms::Response{
            .request_type = RequestType::CREATE_ORDER,
            .origin = Origin::EXCHANGE,
            .request_status = RequestStatus::ACCEPTED,
            .error = {},
            .text = {},
            .version = request.version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        auto order_update = server::oms::OrderUpdate{
            .account = account_.name,
            .exchange = shared_.settings.exchange,
            .symbol = new_order.symbol,
            .side = side,
            .position_effect = {},
            .margin_mode = {},
            .max_show_quantity = NaN,
            .order_type = order_type,
            .time_in_force = time_in_force,
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
        (*this)(event_2, request.user_id, request.order_id, order_update);
        break;
      }
      default: {
        auto &error = message.error;
        auto error_2 = json::guess_error(error.code);
        auto response = server::oms::Response{
            .request_type = RequestType::CREATE_ORDER,
            .origin = Origin::EXCHANGE,
            .request_status = RequestStatus::REJECTED,
            .error = error_2,
            .text = error.msg,
            .version = request.version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        Trace event_2{event, response};
        (*this)(event_2, request.user_id, request.order_id);
      }
    }
    update_rate_limits(event);
  });
}

void OrderEntryWS::operator()(Trace<json::WSAPICancelOrder> const &event, json::WSAPIRequest const &request) {
  profile_.order_cancel_ack([&]() {
    auto &[trace_info, message] = event;
    log::info<2>("message={}, request={}"sv, message, request);
    switch (message.status) {
      case 200: {
        auto &cancel_order = message.result;
        auto side = json::map(cancel_order.side);
        auto order_type = json::map(cancel_order.type);
        auto time_in_force = json::map(cancel_order.time_in_force);
        auto external_order_id = fmt::format("{}"sv, cancel_order.order_id);  // alloc
        auto order_status = json::map(cancel_order.status);
        auto response = server::oms::Response{
            .request_type = RequestType::CANCEL_ORDER,
            .origin = Origin::EXCHANGE,
            .request_status = RequestStatus::ACCEPTED,
            .error = {},
            .text = {},
            .version = request.version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        auto order_update = server::oms::OrderUpdate{
            .account = account_.name,
            .exchange = shared_.settings.exchange,
            .symbol = cancel_order.symbol,
            .side = side,
            .position_effect = {},
            .margin_mode = {},
            .max_show_quantity = NaN,
            .order_type = order_type,
            .time_in_force = time_in_force,
            .execution_instructions = {},
            .create_time_utc = {},
            .update_time_utc = cancel_order.transact_time,
            .external_account = {},
            .external_order_id = external_order_id,
            .client_order_id = {},
            .order_status = order_status,
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
        (*this)(event_2, request.user_id, request.order_id, order_update);
        break;
      }
      default: {
        auto &error = message.error;
        auto error_2 = json::guess_error(error.code);
        auto response = server::oms::Response{
            .request_type = RequestType::CANCEL_ORDER,
            .origin = Origin::EXCHANGE,
            .request_status = RequestStatus::REJECTED,
            .error = error_2,
            .text = error.msg,
            .version = request.version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        Trace event_2{event, response};
        (*this)(event_2, request.user_id, request.order_id);
      }
    }
    update_rate_limits(event);
  });
}

void OrderEntryWS::operator()(Trace<json::WSAPICancelReplaceOrder> const &event, json::WSAPIRequest const &request) {
  auto &[trace_info, message] = event;
  log::info<2>("message={}, request={}"sv, message, request);
  switch (message.status) {
    case 200: {
      Trace event_2{trace_info, message.result};
      update_helper(event_2, request, message.status, {}, {});
      break;
    }
    default:
      log::error("Unexpected: error={}"sv, message.error);
  }
  update_rate_limits(event);
}

// helpers

void OrderEntryWS::update_rate_limits(auto &event) {
  auto &[trace_info, message] = event;
  shared_.rate_limits.clear();
  for (auto &item : message.rate_limits) {
    auto type = [&]() -> RateLimitType {
      switch (item.rate_limit_type) {
        using enum json::RateLimitType::type_t;
        case UNDEFINED__:
        case UNKNOWN__:
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
    if (type == RateLimitType{})
      continue;
    auto period = [&]() -> std::chrono::seconds {
      switch (item.interval) {
        using enum json::Interval::type_t;
        case UNDEFINED__:
        case UNKNOWN__:
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
    shared_.rate_limits.emplace_back(std::move(rate_limit));
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

void OrderEntryWS::update_helper(
    Trace<json::CancelReplaceOrder> const &event,
    json::WSAPIRequest const &request,
    [[maybe_unused]] int32_t status,
    int32_t error_code,
    std::string_view const &error_msg) {
  auto &trace_info = event.trace_info;
  auto &cancel_replace_order = event.value;
  // cancel
  auto dispatch_cancel_error = [&]() {
    auto &cancel_order = cancel_replace_order.cancel_response;
    auto response = server::oms::Response{
        .request_type = RequestType::CANCEL_ORDER,
        .origin = Origin::EXCHANGE,
        .request_status = RequestStatus::REJECTED,
        .error = json::guess_error(std::max(error_code, cancel_order.code)),
        .text = std::empty(cancel_order.msg) ? error_msg : cancel_order.msg,
        .version = request.version,
        .request_id = {},
        .quantity = NaN,
        .price = NaN,
    };
    if (shared_.update_order(
            request.user_id, request.order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
    } else {
      log::warn(
          "Did not find order: user_id={}, order_id={}, version={}"sv,
          request.user_id,
          request.order_id,
          request.version);
    }
  };
  switch (cancel_replace_order.cancel_result) {
    using enum json::SuccessOrFailure::type_t;
    case UNDEFINED__:
      dispatch_cancel_error();
      break;
    case UNKNOWN__:
      log::warn("Unexpected"sv);
      break;
    case SUCCESS: {
      auto &cancel_order = cancel_replace_order.cancel_response;
      auto side = json::map(cancel_order.side);
      auto order_type = json::map(cancel_order.type);
      auto time_in_force = json::map(cancel_order.time_in_force);
      auto external_order_id = fmt::format("{}"sv, cancel_order.order_id);  // alloc
      auto order_status = json::map(cancel_order.status);
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = request.version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      auto order_update = server::oms::OrderUpdate{
          .account = account_.name,
          .exchange = shared_.settings.exchange,
          .symbol = cancel_order.symbol,
          .side = side,
          .position_effect = {},
          .margin_mode = {},
          .max_show_quantity = NaN,
          .order_type = order_type,
          .time_in_force = time_in_force,
          .execution_instructions = {},
          .create_time_utc = {},
          .update_time_utc = {},
          .external_account = {},
          .external_order_id = external_order_id,
          .client_order_id = {},
          .order_status = order_status,
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
      if (shared_.update_order(
              request.user_id,
              request.order_id,
              stream_id_,
              trace_info,
              response,
              order_update,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(
            "Did not find order: user_id={}, order_id={}, version={}"sv,
            request.user_id,
            request.order_id,
            request.version);
      }
      break;
    }
    case FAILURE:
    case NOT_ATTEMPTED:
      dispatch_cancel_error();
      break;
  }
  // new order
  auto dispatch_create_error = [&]() {
    auto &new_order = cancel_replace_order.new_order_response;
    auto response = server::oms::Response{
        .request_type = RequestType::CREATE_ORDER,
        .origin = Origin::EXCHANGE,
        .request_status = RequestStatus::REJECTED,
        .error = json::guess_error(std::max(error_code, new_order.code)),
        .text = std::empty(new_order.msg) ? error_msg : new_order.msg,
        .version = 1,  // note!
        .request_id = {},
        .quantity = NaN,
        .price = NaN,
    };
    if (shared_.update_order(
            request.user_id, request.order_id_2, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {
            })) {
    } else {
      log::warn(
          "Did not find order: request.user_id={}, request.order_id={}, version={}"sv,
          request.user_id,
          request.order_id_2,
          1);
    }
  };
  switch (cancel_replace_order.new_order_result) {
    using enum json::SuccessOrFailure::type_t;
    case UNDEFINED__:
      dispatch_create_error();
      break;
    case UNKNOWN__:
      log::warn("Unexpected"sv);
      break;
    case SUCCESS: {
      auto &new_order = cancel_replace_order.new_order_response;
      auto side = json::map(new_order.side);
      auto order_type = json::map(new_order.type);
      auto time_in_force = json::map(new_order.time_in_force);
      auto external_order_id = fmt::format("{}"sv, new_order.order_id);  // alloc
      auto order_status = json::map(new_order.status);
      auto response = server::oms::Response{
          .request_type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = 1,  // note!
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      auto order_update = server::oms::OrderUpdate{
          .account = account_.name,
          .exchange = shared_.settings.exchange,
          .symbol = new_order.symbol,
          .side = side,
          .position_effect = {},
          .margin_mode = {},
          .max_show_quantity = NaN,
          .order_type = order_type,
          .time_in_force = time_in_force,
          .execution_instructions = {},
          .create_time_utc = {},
          .update_time_utc = {},
          .external_account = {},
          .external_order_id = external_order_id,
          .client_order_id = {},
          .order_status = order_status,
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
      if (shared_.update_order(
              request.user_id,
              request.order_id_2,
              stream_id_,
              trace_info,
              response,
              order_update,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("Did not find order: user_id={}, order_id={}, version={}"sv, request.user_id, request.order_id_2, 1);
      }
      break;
    }
    case FAILURE:
    case NOT_ATTEMPTED:
      dispatch_create_error();
      break;
  }
}

template <typename... Args>
void OrderEntryWS::operator()(
    Trace<server::oms::Response> const &event, uint8_t user_id, uint64_t order_id, Args &&...args) {
  auto &[trace_info, response] = event;
  if (shared_.update_order(
          user_id,
          order_id,
          stream_id_,
          trace_info,
          response,
          std::forward<Args>(args)...,
          []([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
  }
}

void OrderEntryWS::operator()(Trace<server::oms::OrderUpdate> const &event, std::string_view const &client_order_id) {
  auto &[trace_info, order_update] = event;
  if (shared_.update_order(
          client_order_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

}  // namespace binance
}  // namespace roq
