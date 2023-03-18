/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/order_entry_ws.hpp"

#include "roq/mask.hpp"

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
    SupportType::TRADE,
    SupportType::FUNDS,
};

auto const REQUEST_ID = uint32_t{1000000};
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto const &account) {
  return fmt::format("{}:{}:{}"_cf, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &context) {
  auto uri = Flags::ws_api_uri();
  auto config = web::socket::Client::Config{
      .always_reconnect = true,
      .connection_timeout = server::Flags::net_connection_timeout(),
      .disconnect_on_idle_timeout = {},
      .validate_certificate = server::Flags::net_tls_validate_certificate(),
      .uris = {&uri, 1},
      .query = {},
      .ping_frequency = Flags::ws_api_ping_freq(),
      .read_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
  };
  return web::socket::ClientFactory::create(handler, context, config, []() -> std::string { return {}; });
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(auto const &group, auto const &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

// === IMPLEMENTATION ===

OrderEntryWS::OrderEntryWS(
    Handler &handler,
    io::Context &context,
    uint16_t stream_id,
    Authenticator &authenticator,
    Shared &shared,
    Request &request)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, authenticator.get_account())},
      connection_{create_connection(*this, context)}, decode_buffer_{Flags::decode_buffer_size()},
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(name_, "parse"sv),
          .outbound_account_position = create_metrics(name_, "outbound_account_position"sv),
          .balance_update = create_metrics(name_, "balance_update"sv),
          .execution_report = create_metrics(name_, "execution_report"sv),
          .list_status = create_metrics(name_, "list_status"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      authenticator_{authenticator}, shared_{shared}, request_{request}, request_id_{REQUEST_ID} {
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
  ping_listen_key(now);
  if (ready() && !downloading()) {
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
      .write(profile_.outbound_account_position, metrics::PROFILE)
      .write(profile_.balance_update, metrics::PROFILE)
      .write(profile_.execution_report, metrics::PROFILE)
      .write(profile_.list_status, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void OrderEntryWS::operator()(Event<Disconnected> const &) {
}

uint16_t OrderEntryWS::operator()(
    Event<CreateOrder> const &event, oms::Order const &order, std::string_view const &request_id) {
  order_place(event, order, request_id);
  return stream_id_;
}

uint16_t OrderEntryWS::operator()(
    Event<ModifyOrder> const &,
    oms::Order const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  throw oms::NotSupported{"not supported"sv};
}

uint16_t OrderEntryWS::operator()(
    Event<CancelOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  order_cancel(event, order, request_id, previous_request_id);
  return stream_id_;
}

uint16_t OrderEntryWS::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  open_orders_cancel_all(event, request_id);
  return stream_id_;
}

void OrderEntryWS::create_listen_key() {
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}-{}",)"
      R"("method":"userDataStream.start",)"
      R"("params":{{)"
      R"("apiKey":"{}")"
      R"(}})"
      R"(}})"sv,
      ++request_id_,
      magic_enum::enum_name(json::WSAPIType::LISTEN_KEY_CREATE),
      authenticator_.get_key());
  log::debug("{}"sv, message);
  (*connection_).send_text(message);
}

void OrderEntryWS::ping_listen_key(std::chrono::nanoseconds now) {
  if (!ready())
    return;
  if (std::empty(listen_key_))
    return;
  if (listen_key_refresh_ == listen_key_refresh_.zero() || now < listen_key_refresh_)
    return;
  log::info<1>("Refreshing listen key..."sv);
  listen_key_refresh_ = now + Flags::rest_listen_key_refresh();
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}-{}",)"
      R"("method":"userDataStream.ping",)"
      R"("params":{{)"
      R"("apiKey":"{}",)"
      R"("listenKey":"{}")"
      R"(}})"
      R"(}})"sv,
      ++request_id_,
      magic_enum::enum_name(json::WSAPIType::LISTEN_KEY_PING),
      authenticator_.get_key(),
      listen_key_);
  log::debug("{}"sv, message);
  (*connection_).send_text(message);
}

void OrderEntryWS::account_status() {
  auto now = clock::get_realtime<std::chrono::milliseconds>();
  auto message_for_signature = fmt::format("apiKey={}&timestamp={}"sv, authenticator_.get_key(), now.count());
  auto signature = authenticator_.create_ws_api_signature(message_for_signature);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}-{}",)"
      R"("method":"account.status",)"
      R"("params":{{)"
      R"("apiKey":"{}",)"
      R"("timestamp":"{}",)"
      R"("signature":"{}")"
      R"(}})"
      R"(}})"sv,
      ++request_id_,
      magic_enum::enum_name(json::WSAPIType::ACCOUNT_STATUS),
      authenticator_.get_key(),
      now.count(),
      signature);
  log::debug("{}"sv, message);
  (*connection_).send_text(message);
}

void OrderEntryWS::open_orders_status() {
  auto now = clock::get_realtime<std::chrono::milliseconds>();
  auto message_for_signature = fmt::format("apiKey={}&timestamp={}"sv, authenticator_.get_key(), now.count());
  auto signature = authenticator_.create_ws_api_signature(message_for_signature);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}-{}",)"
      R"("method":"openOrders.status",)"
      R"("params":{{)"
      R"("apiKey":"{}",)"
      R"("timestamp":"{}",)"
      R"("signature":"{}")"
      R"(}})"
      R"(}})"sv,
      ++request_id_,
      magic_enum::enum_name(json::WSAPIType::OPEN_ORDERS_STATUS),
      authenticator_.get_key(),
      now.count(),
      signature);
  log::debug("{}"sv, message);
  (*connection_).send_text(message);
}

void OrderEntryWS::open_orders_cancel_all(Event<CancelAllOrders> const &, std::string_view const &request_id) {
  for (auto &symbol : open_orders_symbols_) {
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto message_for_signature =
        fmt::format("apiKey={}&symbol={}&timestamp={}"sv, authenticator_.get_key(), symbol, now.count());
    auto signature = authenticator_.create_ws_api_signature(message_for_signature);
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}-{}",)"
        R"("method":"openOrders.cancelAll",)"
        R"("params":{{)"
        R"("apiKey":"{}",)"
        R"("symbol":"{}",)"
        R"("timestamp":"{}",)"
        R"("signature":"{}")"
        R"(}})"
        R"(}})"sv,
        ++request_id_,
        magic_enum::enum_name(json::WSAPIType::OPEN_ORDERS_CANCEL_ALL),
        authenticator_.get_key(),
        symbol,
        now.count(),
        signature);
    log::debug("{}"sv, message);
    (*connection_).send_text(message);
  }
}

void OrderEntryWS::order_place(
    Event<CreateOrder> const &event, oms::Order const &order, std::string_view const &request_id) {
  auto &[message_info, create_order] = event;
  open_orders_symbols_.emplace(create_order.symbol);
  auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
  auto now = clock::get_realtime<std::chrono::milliseconds>();
  auto message_for_signature = json::new_order_ws_url(
      encode_buffer_, create_order, order, request_id, recv_window, authenticator_.get_key(), now);
  auto signature = authenticator_.create_ws_api_signature(message_for_signature);
  auto params = json::new_order_ws_json(
      encode_buffer_, create_order, order, request_id, recv_window, authenticator_.get_key(), now, signature);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}-{}",)"
      R"("method":"order.place",)"
      R"("params":{})"
      R"(}})"sv,
      ++request_id_,
      magic_enum::enum_name(json::WSAPIType::ORDER_PLACE),
      params);
  log::debug("{}"sv, message);
  (*connection_).send_text(message);
}

void OrderEntryWS::order_cancel(
    Event<CancelOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto &[message_info, cancel_order] = event;
  auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
  auto now = clock::get_realtime<std::chrono::milliseconds>();
  auto message_for_signature = json::cancel_order_ws_url(
      encode_buffer_, cancel_order, order, request_id, previous_request_id, recv_window, authenticator_.get_key(), now);
  auto signature = authenticator_.create_ws_api_signature(message_for_signature);
  auto params = json::cancel_order_ws_json(
      encode_buffer_,
      cancel_order,
      order,
      request_id,
      previous_request_id,
      recv_window,
      authenticator_.get_key(),
      now,
      signature);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}-{}",)"
      R"("method":"order.cancel",)"
      R"("params":{})"
      R"(}})"sv,
      ++request_id_,
      magic_enum::enum_name(json::WSAPIType::ORDER_CANCEL),
      params);
  log::debug("{}"sv, message);
  (*connection_).send_text(message);
}

void OrderEntryWS::operator()(web::socket::Client::Connected const &) {
}

void OrderEntryWS::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
}

void OrderEntryWS::operator()(web::socket::Client::Ready const &) {
  create_listen_key();
  (*this)(ConnectionStatus::LOGIN_SENT);
}

void OrderEntryWS::operator()(web::socket::Client::Close const &) {
}

void OrderEntryWS::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = authenticator_.get_account(),
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
        .account = authenticator_.get_account(),
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::WS,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

void OrderEntryWS::parse(std::string_view const &message) {
  profile_.parse([&]() {
    try {
      TraceInfo trace_info;
      core::json::Buffer buffer{decode_buffer_};
      log::debug(R"(HERE message="{}")"sv, message);
      json::WSAPIParser::dispatch(*this, message, buffer, trace_info);
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void OrderEntryWS::operator()(Trace<json::Error> const &event) {
  auto &[trace_info, error] = event;
  log::debug("HERE {}"sv, error);
}

void OrderEntryWS::operator()(Trace<json::ListenKey> const &event) {
  auto &[trace_info, listen_key] = event;
  log::debug("HERE {}"sv, listen_key);
  listen_key_ = listen_key.listen_key;
  auto listen_key_update = ListenKeyUpdate{
      .account = authenticator_.get_account(),
      .listen_key = listen_key_,
  };
  create_trace_and_dispatch(handler_, trace_info, listen_key_update);
  (*this)(ConnectionStatus::READY);
  auto now = clock::get_system();
  listen_key_refresh_ = now + Flags::rest_listen_key_refresh();
}

void OrderEntryWS::operator()(Trace<json::Account> const &event) {
  auto &[trace_info, account] = event;
  log::debug("HERE {}"sv, account);
  for (auto &item : account.balances) {
    // log::debug("item={}"sv, item);
    auto funds_update = FundsUpdate{
        .stream_id = stream_id_,
        .account = authenticator_.get_account(),
        .currency = item.asset,
        .balance = item.free,
        .hold = item.locked,
        .external_account = {},
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
  request_.respond_account = clock::get_system();  // completion
  download_account_ = false;
}

void OrderEntryWS::operator()(Trace<json::OpenOrders> const &event) {
  auto &[trace_info, open_orders] = event;
  log::debug("HERE {}"sv, open_orders);
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
        .account = authenticator_.get_account(),
        .exchange = Flags::exchange(),
        .symbol = order.symbol,
        .side = side,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = order_type,
        .time_in_force = time_in_force,
        .execution_instructions = {},
        .order_template = {},
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
    };
    Trace event_2{trace_info, order_update};
    (*this)(event_2, order.client_order_id);
  }
  request_.respond_orders = clock::get_system();  // completion
  download_orders_ = false;
}

void OrderEntryWS::operator()(Trace<json::NewOrder> const &event) {
  auto &[trace_info, new_order] = event;
  // log::info<2>("new_order={}, user_id={}, order_id={}, version={}"sv, new_order, user_id, order_id, version);
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
      .version = {},
      .request_id = {},
      .quantity = NaN,
      .price = NaN,
  };
  auto order_update = oms::OrderUpdate{
      .account = authenticator_.get_account(),
      .exchange = Flags::exchange(),
      .symbol = new_order.symbol,
      .side = side,
      .position_effect = {},
      .max_show_quantity = NaN,
      .order_type = order_type,
      .time_in_force = time_in_force,
      .execution_instructions = {},
      .order_template = {},
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
  };
  Trace event_2{trace_info, response};
  (*this)(event_2, new_order.client_order_id, order_update);
}

void OrderEntryWS::operator()(Trace<json::CancelOrder> const &event) {
  auto &[trace_info, cancel_order] = event;
  // log::info<2>("cancel_order={}, user_id={}, order_id={}, version={}"sv, cancel_order, user_id, order_id, version);
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
      .version = {},  // XXX we can encode this in the id field
      .request_id = {},
      .quantity = NaN,
      .price = NaN,
  };
  auto order_update = oms::OrderUpdate{
      .account = authenticator_.get_account(),
      .exchange = Flags::exchange(),
      .symbol = cancel_order.symbol,
      .side = side,
      .position_effect = {},
      .max_show_quantity = NaN,
      .order_type = order_type,
      .time_in_force = time_in_force,
      .execution_instructions = {},
      .order_template = {},
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
  };
  Trace event_2{trace_info, response};
  (*this)(event_2, cancel_order.client_order_id, order_update);
}

void OrderEntryWS::operator()(Trace<json::CancelAllOpenOrders> const &event) {
  auto &[trace_info, cancel_all_open_orders] = event;
  log::debug("HERE {}"sv, cancel_all_open_orders);
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
        .account = authenticator_.get_account(),
        .exchange = Flags::exchange(),
        .symbol = order.symbol,
        .side = side,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = order_type,
        .time_in_force = time_in_force,
        .execution_instructions = {},
        .order_template = {},
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
    };
    shared_.update_order(
        order.client_order_id, stream_id_, trace_info, order_update, []([[maybe_unused]] auto &order) {});
  }
}

template <typename... Args>
void OrderEntryWS::operator()(
    Trace<oms::Response> const &event,
    std::string_view const &client_order_id,
    oms::OrderUpdate const &order_update,
    Args &&...args) {
  auto &[trace_info, response] = event;
  if (shared_.update_order(
          client_order_id, stream_id_, trace_info, response, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

template <typename... Args>
void OrderEntryWS::operator()(
    Trace<oms::OrderUpdate> const &event, std::string_view const &client_order_id, Args &&...args) {
  auto &[trace_info, order_update] = event;
  if (shared_.update_order(
          client_order_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

}  // namespace binance
}  // namespace roq
