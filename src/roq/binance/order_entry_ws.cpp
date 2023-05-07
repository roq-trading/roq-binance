/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/order_entry_ws.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/socket/client_factory.hpp"

#include "roq/binance/flags.hpp"

#include "roq/binance/json/utils.hpp"
#include "roq/binance/json/ws_api_type.hpp"

using namespace std::literals;

using namespace fmt::literals;

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
auto create_name(auto stream_id, auto const &account) {
  return fmt::format("{}:{}:{}"_cf, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &settings, auto &context, auto &interface) {
  auto uri = Flags::ws_api_uri();
  auto config = web::socket::Client::Config{
      // connection
      .interface = interface,
      .uris = {&uri, 1},
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
      .ping_frequency = Flags::ws_api_ping_freq(),
      // implementation
      .decode_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
  };
  return web::socket::ClientFactory::create(handler, context, config, []() -> std::string { return {}; });
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(auto &settings, auto const &group, auto const &function)
      : core::metrics::Factory(settings.app.name, group, function) {}
};
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
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.get_name())}, master_{master},
      connection_{create_connection(*this, shared.settings, context, interface)},
      decode_buffer_{Flags::decode_buffer_size()},
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
      account_{account}, shared_{shared}, request_{request}, request_id_{REQUEST_ID} {
  // DEBUG
  open_orders_symbols_.emplace("BTCUSDT"sv);
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
  }
}

void OrderEntryWS::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.parse, metrics::PROFILE)
      .write(profile_.error, metrics::PROFILE)
      .write(profile_.user_data_stream_start, metrics::PROFILE)
      .write(profile_.user_data_stream_start_ack, metrics::PROFILE)
      .write(profile_.user_data_stream_ping, metrics::PROFILE)
      .write(profile_.user_data_stream_ping_ack, metrics::PROFILE)
      .write(profile_.account_status, metrics::PROFILE)
      .write(profile_.account_status_ack, metrics::PROFILE)
      .write(profile_.open_orders_status, metrics::PROFILE)
      .write(profile_.open_orders_status_ack, metrics::PROFILE)
      .write(profile_.open_orders_cancel_all, metrics::PROFILE)
      .write(profile_.open_orders_cancel_all_ack, metrics::PROFILE)
      .write(profile_.order_place, metrics::PROFILE)
      .write(profile_.order_place_ack, metrics::PROFILE)
      .write(profile_.order_cancel, metrics::PROFILE)
      .write(profile_.order_cancel_ack, metrics::PROFILE)
      .write(profile_.order_cancel_replace, metrics::PROFILE)
      .write(profile_.order_cancel_replace_ack, metrics::PROFILE)
      .write(profile_.order_cancel_replace_error, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void OrderEntryWS::operator()(Event<Disconnected> const &event) {
  auto user_id = event.message_info.source;
  account_.cancel_order_request_buffer_[user_id].reset();
}

uint16_t OrderEntryWS::operator()(
    Event<CreateOrder> const &event, oms::Order const &order, std::string_view const &request_id) {
  auto &message_info = event.message_info;
  auto &tmp = account_.cancel_order_request_buffer_[message_info.source];
  if (!tmp) {
    order_place(event, order, request_id);
  } else {
    // cancel + replace
    typename std::remove_cvref<decltype(tmp)>::type tmp2;
    tmp.swap(tmp2);
    // XXX HANS do we get error on cancel if we can't send?
    order_cancel_replace(*tmp2, event, order, request_id);
  }
  return stream_id_;
}

uint16_t OrderEntryWS::operator()(
    Event<ModifyOrder> const &,
    oms::Order const &,
    [[maybe_unused]] std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id) {
  throw oms::NotSupported{"not supported"sv};
}

uint16_t OrderEntryWS::operator()(
    Event<CancelOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto &[message_info, cancel_order] = event;
  auto &tmp = account_.cancel_order_request_buffer_[message_info.source];
  if (tmp)
    throw oms::NotSupported{"not supported"sv};
  if (message_info.is_last) {
    order_cancel(event, order, request_id, previous_request_id);
  } else {
    // cancel + replace
    auto cancel_order_request = server::cache::CancelOrderRequest{
        .cancel_order = cancel_order,
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

void OrderEntryWS::user_data_stream_start() {
  profile_.user_data_stream_start([&]() {
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::LISTEN_KEY_CREATE,
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
    log::debug("{}"sv, message);
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
    listen_key_refresh_ = now + Flags::rest_listen_key_refresh();
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::LISTEN_KEY_PING,
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
    log::debug("{}"sv, message);
    (*connection_).send_text(message);
  });
}

void OrderEntryWS::account_status() {
  profile_.account_status([&]() {
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto message_for_signature = fmt::format("apiKey={}&timestamp={}"sv, account_.get_key(), now.count());
    auto signature = account_.create_ws_api_signature(message_for_signature);
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::ACCOUNT_STATUS,
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
    log::debug("{}"sv, message);
    (*connection_).send_text(message);
  });
}

void OrderEntryWS::open_orders_status() {
  profile_.open_orders_status([&]() {
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto message_for_signature = fmt::format("apiKey={}&timestamp={}"sv, account_.get_key(), now.count());
    auto signature = account_.create_ws_api_signature(message_for_signature);
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::OPEN_ORDERS_STATUS,
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
    log::debug("{}"sv, message);
    (*connection_).send_text(message);
  });
}

void OrderEntryWS::open_orders_cancel_all(
    Event<CancelAllOrders> const &event, [[maybe_unused]] std::string_view const &request_id) {
  profile_.open_orders_cancel_all([&]() {
    if (!ready()) {
      log::warn("Unable to cancel open orders (reason: NOT READY)"sv);
      return;
    }
    auto &[message_info, cancel_all_orders] = event;
    for (auto &symbol : open_orders_symbols_) {
      auto now = clock::get_realtime<std::chrono::milliseconds>();
      auto message_for_signature =
          fmt::format("apiKey={}&symbol={}&timestamp={}"sv, account_.get_key(), symbol, now.count());
      auto signature = account_.create_ws_api_signature(message_for_signature);
      auto request = json::WSAPIRequest{
          .sequence = ++request_id_,
          .type = json::WSAPIType::OPEN_ORDERS_CANCEL_ALL,
          .user_id = message_info.source,
      };
      auto request_id_2 = json::WSAPIRequest::encode(request_encode_buffer_, request);
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
      log::debug("{}"sv, message);
      (*connection_).send_text(message);
    }
  });
}

void OrderEntryWS::order_place(
    Event<CreateOrder> const &event, oms::Order const &order, [[maybe_unused]] std::string_view const &request_id) {
  profile_.order_place([&]() {
    if (!ready())
      throw oms::NotReady{"not ready"sv};
    auto &[message_info, create_order] = event;
    open_orders_symbols_.emplace(create_order.symbol);
    auto &create_order_template = shared_.get_create_order_template(create_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto message_for_signature = json::new_order_ws_url(
        encode_buffer_, create_order, order, request_id, create_order_template, recv_window, account_.get_key(), now);
    log::debug(R"(message_for_signature="{}")"sv, message_for_signature);
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
    log::debug(R"(params="{}")"sv, params);
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::ORDER_PLACE,
        .user_id = message_info.source,
        .order_id = create_order.order_id,
        .version = 1,
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
    log::debug("{}"sv, message);
    (*connection_).send_text(message);
  });
}

void OrderEntryWS::order_cancel(
    Event<CancelOrder> const &event,
    oms::Order const &order,
    [[maybe_unused]] std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id) {
  profile_.order_cancel([&]() {
    if (!ready())
      throw oms::NotReady{"not ready"sv};
    auto &[message_info, cancel_order] = event;
    auto &cancel_order_template = shared_.get_cancel_order_template(cancel_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
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
    log::debug(R"(message_for_signature="{}")"sv, message_for_signature);
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
    log::debug(R"(params="{}")"sv, params);
    auto request = json::WSAPIRequest{
        .sequence = ++request_id_,
        .type = json::WSAPIType::ORDER_CANCEL,
        .user_id = message_info.source,
        .order_id = cancel_order.order_id,
        .version = cancel_order.version,
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
    log::debug("{}"sv, message);
    (*connection_).send_text(message);
  });
}

void OrderEntryWS::order_cancel_replace(
    server::cache::CancelOrderRequest const &cancel_order_request,
    Event<CreateOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id) {
  profile_.order_cancel_replace([&]() {
    if (!ready())
      throw oms::NotReady{"not ready"sv};
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
                  utils::safe_cast(Flags::rest_order_recv_window()),
                  flags::Flags::cancel_replace_stop_on_failure(),
                  account_.get_key(),
                  now);
              log::debug(R"(message_for_signature="{}")"sv, message_for_signature);
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
                  utils::safe_cast(Flags::rest_order_recv_window()),
                  flags::Flags::cancel_replace_stop_on_failure(),
                  account_.get_key(),
                  now,
                  signature);
              log::debug(R"(params="{}")"sv, params);
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
              log::debug("{}"sv, message);
              (*connection_).send_text(message);
            })) {
    } else {
      throw oms::Rejected{Origin::GATEWAY, Error::CONDITIONAL_REQUEST_HAS_FAILED, "internal error"sv};
    }
  });
}

void OrderEntryWS::operator()(web::socket::Client::Connected const &) {
}

void OrderEntryWS::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
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
      .account = account_.get_name(),
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
        .account = account_.get_name(),
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
    try {
      log::debug(R"(message="{}")"sv, message);
      TraceInfo trace_info;
      core::json::Buffer buffer{decode_buffer_};
      auto res = json::WSAPIParser::dispatch(*this, message, buffer, trace_info);
      if (!res) [[unlikely]] {
        log::warn(R"(message="{}")"sv, message);
      }
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void OrderEntryWS::operator()(Trace<json::Error> const &event, json::WSAPIRequest const &request, int32_t status) {
  profile_.error([&]() {
    auto &error = event.value;
    log::warn<2>("error={}, request={}, status={}"sv, error, request, status);
    auto dispatch_oms_error = [&](auto type) {
      auto error_2 = json::guess_error(error.code);
      auto response = oms::Response{
          .type = type,
          .origin = Origin::EXCHANGE,
          .status = RequestStatus::REJECTED,
          .error = error_2,
          .text = error.msg,
          .version = request.version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, request.user_id, request.order_id);
    };
    switch (request.type) {
      using enum json::WSAPIType::type_t;
      case UNDEFINED__:
      case UNKNOWN__:
        break;
      case LISTEN_KEY_CREATE:
        switch (error.code) {
          case -2015:  // invalid key
            log::fatal("Unexpected: error={}"sv, error);
        }
        break;
      case LISTEN_KEY_PING:
        break;
      case ACCOUNT_STATUS:
        request_.respond_account = clock::get_system();  // completion
        download_account_ = false;
        break;
      case OPEN_ORDERS_STATUS:
        request_.respond_orders = clock::get_system();  // completion
        download_orders_ = false;
        break;
      case OPEN_ORDERS_CANCEL_ALL:
        break;
      case ORDER_PLACE:
        dispatch_oms_error(RequestType::CREATE_ORDER);
        break;
      case ORDER_CANCEL:
        dispatch_oms_error(RequestType::CANCEL_ORDER);
        break;
      case ORDER_CANCEL_REPLACE:
        log::warn("Unexpected"sv);
        break;
    }
  });
}

void OrderEntryWS::operator()(Trace<json::ListenKey> const &event, json::WSAPIRequest const &request, int32_t status) {
  profile_.user_data_stream_start_ack([&]() {
    auto &[trace_info, listen_key] = event;
    log::info<2>("listen_key={}, request={}, status={}"sv, listen_key, request, status);
    listen_key_ = listen_key.listen_key;
    auto listen_key_update = ListenKeyUpdate{
        .account = account_.get_name(),
        .listen_key = listen_key_,
    };
    create_trace_and_dispatch(handler_, trace_info, listen_key_update);
    (*this)(ConnectionStatus::READY);
    auto now = clock::get_system();
    listen_key_refresh_ = now + Flags::rest_listen_key_refresh();
  });
}

void OrderEntryWS::operator()(Trace<json::Account> const &event, json::WSAPIRequest const &request, int32_t status) {
  profile_.user_data_stream_ping_ack([&]() {
    auto &[trace_info, account] = event;
    log::info<2>("account={}, request={}, status={}"sv, account, request, status);
    for (auto &item : account.balances) {
      // log::debug("item={}"sv, item);
      auto funds_update = FundsUpdate{
          .stream_id = stream_id_,
          .account = account_.get_name(),
          .currency = item.asset,
          .balance = item.free,
          .hold = item.locked,
          .external_account = {},
          .update_type = UpdateType::SNAPSHOT,
          .exchange_time_utc = account.update_time,
          .sending_time_utc = account.update_time,
      };
      create_trace_and_dispatch(handler_, trace_info, funds_update, true);
    }
    request_.respond_account = clock::get_system();  // completion
    download_account_ = false;
  });
}

void OrderEntryWS::operator()(Trace<json::OpenOrders> const &event, json::WSAPIRequest const &request, int32_t status) {
  profile_.open_orders_status_ack([&]() {
    auto &[trace_info, open_orders] = event;
    log::info<2>("open_orders={}, request={}, status={}"sv, open_orders, request, status);
    for (auto &order : open_orders.data) {
      log::info<2>("order={}"sv, order);
      if (std::empty(order.client_order_id))
        continue;
      open_orders_symbols_.emplace(order.symbol);
      auto side = json::map(order.side);
      auto order_type = json::map(order.type);
      auto time_in_force = json::map(order.time_in_force);
      auto external_order_id = fmt::format("{}"_cf, order.order_id);  // alloc
      auto order_status = json::map(order.status);
      auto order_update = oms::OrderUpdate{
          .account = account_.get_name(),
          .exchange = Flags::exchange(),
          .symbol = order.symbol,
          .side = side,
          .position_effect = {},
          .max_show_quantity = NaN,
          .order_type = order_type,
          .time_in_force = time_in_force,
          .execution_instructions = {},
          .create_time_utc = order.time,
          .update_time_utc = order.update_time,
          .external_account = {},
          .external_order_id = external_order_id,
          .status = order_status,
          .quantity = order.orig_qty,
          .price = order.price,
          .stop_price = order.stop_price,
          .remaining_quantity = NaN,
          .traded_quantity = order.executed_qty,
          .average_traded_price = {},
          .last_traded_quantity = {},
          .last_traded_price = {},
          .last_liquidity = {},
          .update_type = UpdateType::SNAPSHOT,
          .sending_time_utc = {},
      };
      Trace event_2{trace_info, order_update};
      (*this)(event_2, order.client_order_id);
    }
    request_.respond_orders = clock::get_system();  // completion
    download_orders_ = false;
  });
}

void OrderEntryWS::operator()(
    Trace<json::CancelAllOpenOrders> const &event, json::WSAPIRequest const &request, int32_t status) {
  profile_.open_orders_cancel_all_ack([&]() {
    auto &[trace_info, cancel_all_open_orders] = event;
    log::info<2>("cancel_all_open_orders={}, request={}, status={}"sv, cancel_all_open_orders, request, status);
    for (auto &order : cancel_all_open_orders.data) {
      log::debug("order={}"sv, order);
      if (std::empty(order.client_order_id))
        continue;
      auto side = json::map(order.side);
      auto order_type = json::map(order.type);
      auto time_in_force = json::map(order.time_in_force);
      auto external_order_id = fmt::format("{}"_cf, order.order_id);  // alloc
      auto order_status = json::map(order.status);
      auto order_update = oms::OrderUpdate{
          .account = account_.get_name(),
          .exchange = Flags::exchange(),
          .symbol = order.symbol,
          .side = side,
          .position_effect = {},
          .max_show_quantity = NaN,
          .order_type = order_type,
          .time_in_force = time_in_force,
          .execution_instructions = {},
          .create_time_utc = order.time,
          .update_time_utc = order.update_time,
          .external_account = {},
          .external_order_id = external_order_id,
          .status = order_status,
          .quantity = order.orig_qty,
          .price = order.price,
          .stop_price = order.stop_price,
          .remaining_quantity = NaN,
          .traded_quantity = order.executed_qty,
          .average_traded_price = {},
          .last_traded_quantity = {},
          .last_traded_price = {},
          .last_liquidity = {},
          .update_type = UpdateType::INCREMENTAL,
          .sending_time_utc = {},
      };
      shared_.update_order(
          order.client_order_id, stream_id_, trace_info, order_update, []([[maybe_unused]] auto &order) {});
    }
  });
}

void OrderEntryWS::operator()(Trace<json::NewOrder> const &event, json::WSAPIRequest const &request, int32_t status) {
  profile_.order_place_ack([&]() {
    auto &[trace_info, new_order] = event;
    log::info<2>("new_order={}, request={}, status={}"sv, new_order, request, status);
    auto side = json::map(new_order.side);
    auto order_type = json::map(new_order.type);
    auto time_in_force = json::map(new_order.time_in_force);
    auto external_order_id = fmt::format("{}"_cf, new_order.order_id);  // alloc
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
    auto response = oms::Response{
        .type = RequestType::CREATE_ORDER,
        .origin = Origin::EXCHANGE,
        .status = RequestStatus::ACCEPTED,
        .error = {},
        .text = {},
        .version = request.version,
        .request_id = {},
        .quantity = NaN,
        .price = NaN,
    };
    auto order_update = oms::OrderUpdate{
        .account = account_.get_name(),
        .exchange = Flags::exchange(),
        .symbol = new_order.symbol,
        .side = side,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = order_type,
        .time_in_force = time_in_force,
        .execution_instructions = {},
        .create_time_utc = {},
        .update_time_utc = new_order.transact_time,
        .external_account = {},
        .external_order_id = external_order_id,
        .status = order_status,
        .quantity = new_order.orig_qty,
        .price = new_order.price,
        .stop_price = NaN,
        .remaining_quantity = remaining_quantity,
        .traded_quantity = new_order.executed_qty,
        .average_traded_price = average_traded_price,
        .last_traded_quantity = last_traded_quantity,
        .last_traded_price = last_traded_price,
        .last_liquidity = {},
        .update_type = UpdateType::INCREMENTAL,
        .sending_time_utc = {},
    };
    Trace event_2{trace_info, response};
    (*this)(event_2, request.user_id, request.order_id, order_update);
  });
}

void OrderEntryWS::operator()(
    Trace<json::CancelOrder> const &event, json::WSAPIRequest const &request, int32_t status) {
  profile_.order_cancel_ack([&]() {
    auto &[trace_info, cancel_order] = event;
    log::info<2>("cancel_order={}, request={}, status={}"sv, cancel_order, request, status);
    auto side = json::map(cancel_order.side);
    auto order_type = json::map(cancel_order.type);
    auto time_in_force = json::map(cancel_order.time_in_force);
    auto external_order_id = fmt::format("{}"_cf, cancel_order.order_id);  // alloc
    auto order_status = json::map(cancel_order.status);
    auto response = oms::Response{
        .type = RequestType::CANCEL_ORDER,
        .origin = Origin::EXCHANGE,
        .status = RequestStatus::ACCEPTED,
        .error = {},
        .text = {},
        .version = request.version,
        .request_id = {},
        .quantity = NaN,
        .price = NaN,
    };
    auto order_update = oms::OrderUpdate{
        .account = account_.get_name(),
        .exchange = Flags::exchange(),
        .symbol = cancel_order.symbol,
        .side = side,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = order_type,
        .time_in_force = time_in_force,
        .execution_instructions = {},
        .create_time_utc = {},
        .update_time_utc = {},
        .external_account = {},
        .external_order_id = external_order_id,
        .status = order_status,
        .quantity = cancel_order.orig_qty,
        .price = cancel_order.price,
        .stop_price = NaN,
        .remaining_quantity = NaN,
        .traded_quantity = cancel_order.executed_qty,
        .average_traded_price = NaN,
        .last_traded_quantity = NaN,
        .last_traded_price = NaN,
        .last_liquidity = {},
        .update_type = UpdateType::INCREMENTAL,
        .sending_time_utc = {},
    };
    Trace event_2{trace_info, response};
    (*this)(event_2, request.user_id, request.order_id, order_update);
  });
}

void OrderEntryWS::operator()(
    Trace<json::CancelReplaceOrder> const &event, json::WSAPIRequest const &request, int32_t status) {
  auto &[trace_info, cancel_replace_order] = event;
  log::info<2>("cancel_replace_order={}, request={}, status={}"sv, cancel_replace_order, request, status);
  update_helper(event, request, status, {}, {});
}

void OrderEntryWS::operator()(
    Trace<json::CancelReplaceOrderError> const &event, json::WSAPIRequest const &request, int32_t status) {
  auto &[trace_info, cancel_replace_order_error] = event;
  log::info<2>("cancel_replace_order_error={}, request={}, status={}"sv, cancel_replace_order_error, request, status);
  assert(cancel_replace_order_error.code != 0);
  Trace event_2{trace_info, cancel_replace_order_error.data};
  update_helper(event_2, request, status, cancel_replace_order_error.code, cancel_replace_order_error.msg);
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
    auto response = oms::Response{
        .type = RequestType::CANCEL_ORDER,
        .origin = Origin::EXCHANGE,
        .status = RequestStatus::REJECTED,
        .error = json::guess_error(std::max(error_code, cancel_order.code)),
        .text = std::empty(cancel_order.msg) ? error_msg : cancel_order.msg,
        .version = request.version,
        .request_id = {},
        .quantity = NaN,
        .price = NaN,
    };
    log::debug("response={}, user_id={}, order_id={}"sv, response, request.user_id, request.order_id);
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
      auto external_order_id = fmt::format("{}"_cf, cancel_order.order_id);  // alloc
      auto order_status = json::map(cancel_order.status);
      auto response = oms::Response{
          .type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = request.version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      auto order_update = oms::OrderUpdate{
          .account = account_.get_name(),
          .exchange = Flags::exchange(),
          .symbol = cancel_order.symbol,
          .side = side,
          .position_effect = {},
          .max_show_quantity = NaN,
          .order_type = order_type,
          .time_in_force = time_in_force,
          .execution_instructions = {},
          .create_time_utc = {},
          .update_time_utc = {},
          .external_account = {},
          .external_order_id = external_order_id,
          .status = order_status,
          .quantity = cancel_order.orig_qty,
          .price = cancel_order.price,
          .stop_price = NaN,
          .remaining_quantity = NaN,
          .traded_quantity = cancel_order.executed_qty,
          .average_traded_price = NaN,
          .last_traded_quantity = NaN,
          .last_traded_price = NaN,
          .last_liquidity = {},
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
    auto response = oms::Response{
        .type = RequestType::CREATE_ORDER,
        .origin = Origin::EXCHANGE,
        .status = RequestStatus::REJECTED,
        .error = json::guess_error(std::max(error_code, new_order.code)),
        .text = std::empty(new_order.msg) ? error_msg : new_order.msg,
        .version = 1,  // note!
        .request_id = {},
        .quantity = NaN,
        .price = NaN,
    };
    log::debug("response={}, user_id={}, order_id={}"sv, response, request.user_id, request.order_id_2);
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
      auto external_order_id = fmt::format("{}"_cf, new_order.order_id);  // alloc
      auto order_status = json::map(new_order.status);
      auto response = oms::Response{
          .type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = 1,  // note!
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      auto order_update = oms::OrderUpdate{
          .account = account_.get_name(),
          .exchange = Flags::exchange(),
          .symbol = new_order.symbol,
          .side = side,
          .position_effect = {},
          .max_show_quantity = NaN,
          .order_type = order_type,
          .time_in_force = time_in_force,
          .execution_instructions = {},
          .create_time_utc = {},
          .update_time_utc = {},
          .external_account = {},
          .external_order_id = external_order_id,
          .status = order_status,
          .quantity = new_order.orig_qty,
          .price = new_order.price,
          .stop_price = NaN,
          .remaining_quantity = NaN,
          .traded_quantity = new_order.executed_qty,
          .average_traded_price = NaN,
          .last_traded_quantity = NaN,
          .last_traded_price = NaN,
          .last_liquidity = {},
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
void OrderEntryWS::operator()(Trace<oms::Response> const &event, uint8_t user_id, uint32_t order_id, Args &&...args) {
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

void OrderEntryWS::operator()(Trace<oms::OrderUpdate> const &event, std::string_view const &client_order_id) {
  auto &[trace_info, order_update] = event;
  if (shared_.update_order(
          client_order_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

}  // namespace binance
}  // namespace roq
