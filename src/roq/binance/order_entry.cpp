/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/order_entry.hpp"

#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/number.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/core/back_emplacer.hpp"
#include "roq/core/charconv.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/rest/client_factory.hpp"

#include "roq/binance/flags.hpp"

#include "roq/binance/json/error.hpp"
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
auto create_name(auto stream_id, auto const &account) {
  return fmt::format("{}:{}:{}"sv, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &context) {
  auto uri = Flags::rest_uri();
  web::rest::Client::Config config{
      .decode_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
      .validate_certificate = server::Flags::net_tls_validate_certificate(),
      .uris = {&uri, 1},
      .proxy = Flags::rest_proxy(),
      .user_agent = ROQ_PACKAGE_NAME,
      .connection = web::http::Connection::KEEP_ALIVE,
      .allow_pipelining = true,
      .request_timeout = Flags::rest_request_timeout(),
      .ping_frequency = Flags::rest_ping_freq(),
      .ping_path = Flags::rest_ping_path(),
  };
  return web::rest::ClientFactory::create(handler, context, config);
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(auto const &group, auto const &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

// === IMPLEMENTATION ===

OrderEntry::OrderEntry(
    Handler &handler, io::Context &context, uint16_t stream_id, Security &security, Shared &shared, Request &request)
    : handler_(handler), stream_id_(stream_id), name_(create_name(stream_id_, security.get_account())),
      connection_(create_connection(*this, context)), decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .listen_key = create_metrics(name_, "listen_key"sv),
          .listen_key_ack = create_metrics(name_, "listen_key_ack"sv),
          .account = create_metrics(name_, "account"sv),
          .account_ack = create_metrics(name_, "account_ack"sv),
          .open_orders = create_metrics(name_, "open_orders"sv),
          .open_orders_ack = create_metrics(name_, "open_orders_ack"sv),
          .new_order = create_metrics(name_, "new_order"sv),
          .new_order_ack = create_metrics(name_, "new_order_ack"sv),
          .cancel_replace_order = create_metrics(name_, "cancel_replace_order"sv),
          .cancel_replace_order_ack = create_metrics(name_, "cancel_replace_order_ack"sv),
          .cancel_order = create_metrics(name_, "cancel_order"sv),
          .cancel_order_ack = create_metrics(name_, "cancel_order_ack"sv),
          .cancel_all_open_orders = create_metrics(name_, "cancel_all_open_orders"sv),
          .cancel_all_open_orders_ack = create_metrics(name_, "cancel_all_open_orders_ack"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
      },
      security_(security), shared_(shared), request_(request),
      download_(Flags::rest_request_timeout(), [this](auto state) { return download(state); }),
      hold_cancel_order_(256) {
}

void OrderEntry::operator()(Event<Start> const &) {
  (*connection_).start();
}

void OrderEntry::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void OrderEntry::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  refresh_listen_key(now);
  if (ready() && !downloading()) {
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
  }
}

void OrderEntry::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.listen_key, metrics::PROFILE)
      .write(profile_.listen_key_ack, metrics::PROFILE)
      .write(profile_.account, metrics::PROFILE)
      .write(profile_.account_ack, metrics::PROFILE)
      .write(profile_.open_orders, metrics::PROFILE)
      .write(profile_.open_orders_ack, metrics::PROFILE)
      .write(profile_.new_order, metrics::PROFILE)
      .write(profile_.new_order_ack, metrics::PROFILE)
      .write(profile_.cancel_replace_order, metrics::PROFILE)
      .write(profile_.cancel_replace_order_ack, metrics::PROFILE)
      .write(profile_.cancel_order, metrics::PROFILE)
      .write(profile_.cancel_order_ack, metrics::PROFILE)
      .write(profile_.cancel_all_open_orders, metrics::PROFILE)
      .write(profile_.cancel_all_open_orders_ack, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY);
}

void OrderEntry::operator()(Event<Disconnected> const &event) {
  auto user_id = event.message_info.source;
  hold_cancel_order_[user_id].reset();
}

uint16_t OrderEntry::operator()(
    Event<CreateOrder> const &event, oms::Order const &order, std::string_view const &request_id) {
  auto &message_info = event.message_info;
  auto &tmp = hold_cancel_order_[message_info.source];
  if (!tmp) {
    new_order(event, order, request_id);
  } else {
    // cancel + replace
    typename std::remove_cvref<decltype(tmp)>::type tmp2;
    tmp.swap(tmp2);
    cancel_replace_order(*tmp2, event, order, request_id);
  }
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    Event<ModifyOrder> const &,
    oms::Order const &,
    [[maybe_unused]] std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id) {
  throw oms::NotSupported{"not supported"sv};
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    Event<CancelOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto &[message_info, cancel_order] = event;
  auto &tmp = hold_cancel_order_[message_info.source];
  if (tmp)
    throw oms::NotSupported{"not supported"sv};
  if (message_info.is_last) {
    (*this).cancel_order(event, order, request_id, previous_request_id);
  } else {
    // cancel + replace
    HoldCancelOrder hold{cancel_order, request_id, previous_request_id, order.symbol};
    tmp = std::make_unique<HoldCancelOrder>(std::move(hold));
  }
  return stream_id_;
}

uint16_t OrderEntry::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  cancel_all_open_orders(event, request_id);
  return stream_id_;
}

void OrderEntry::operator()(web::rest::Client::Connected const &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void OrderEntry::operator()(web::rest::Client::Disconnected const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading())
    download_.reset();
  download_account_ = false;
  download_orders_ = false;
}

void OrderEntry::operator()(web::rest::Client::Latency const &latency) {
  TraceInfo trace_info;
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .account = security_.get_account(),
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntry::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = security_.get_account(),
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::HTTP,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t OrderEntry::download(OrderEntryState state) {
  switch (state) {
    using enum OrderEntryState;
    case UNDEFINED:
      assert(false);
      break;
    case LISTEN_KEY:
      get_listen_key();
      return 1;
    case DONE:
      (*this)(ConnectionStatus::READY);
      assert(!ready_);
      ready_ = true;
      return {};
  }
  assert(false);
  return {};
}

// listen-key

void OrderEntry::get_listen_key() {
  profile_.listen_key([&]() {
    auto headers = security_.create_headers();
    web::rest::Request request{
        .method = web::http::Method::POST,
        .path = "/api/v3/userDataStream"sv,
        .query = {},
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this, sequence = download_.sequence()]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_listen_key_ack(event, sequence);
    };
    (*connection_)("listen_key"sv, request, callback);
  });
}

void OrderEntry::get_listen_key_ack(Trace<web::rest::Response> const &event, [[maybe_unused]] uint32_t sequence) {
  auto const constexpr STATE = OrderEntryState::LISTEN_KEY;
  profile_.listen_key_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::ListenKey listen_key{body};
      log::debug("listen_key={}"sv, listen_key);
      Trace event_2{event, listen_key};
      (*this)(event_2);
      download_.check_relaxed(STATE);
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      if (download_.downloading())
        download_.retry(STATE);
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntry::operator()(Trace<json::ListenKey> const &event) {
  auto &[trace_info, listen_key] = event;
  log::info<2>("listen_key={}"sv, listen_key);
  bool initial = std::empty(listen_key_);
  if (utils::update(listen_key_, listen_key.listen_key)) {
    if (initial) {
      log::info<1>(R"(Listen key has been acquired (value="{}"))"sv, listen_key_);
      ListenKeyUpdate listen_key_update{
          .account = security_.get_account(),
          .listen_key = listen_key.listen_key,
      };
      create_trace_and_dispatch(handler_, trace_info, listen_key_update);
    } else {
      log::info<1>("Listen key has been refreshed!"sv);
    }
  }
  auto now = clock::get_system();
  listen_key_refresh_ = now + Flags::rest_listen_key_refresh();
}

// account

void OrderEntry::get_account() {
  profile_.account([&]() {
    auto query = security_.create_query();
    auto headers = security_.create_headers();
    web::rest::Request request{
        .method = web::http::Method::GET,
        .path = "/api/v3/account"sv,
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

void OrderEntry::get_account_ack(Trace<web::rest::Response> const &event) {
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
      download_account_ = false;
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntry::operator()(Trace<json::Account> const &event) {
  auto &[trace_info, account] = event;
  log::info<2>("account={}"sv, account);
  for (auto &item : account.balances) {
    // log::debug("item={}"sv, item);
    FundsUpdate funds_update{
        .stream_id = stream_id_,
        .account = security_.get_account(),
        .currency = item.asset,
        .balance = item.free,
        .hold = item.locked,
        .external_account = {},
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

// orders

void OrderEntry::get_open_orders() {
  profile_.open_orders([&]() {
    auto query = security_.create_query();
    auto headers = security_.create_headers();
    auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
    encode_buffer_.clear();
    fmt::format_to(
        std::back_inserter(encode_buffer_),
        R"({{)"
        R"("timestamp":{})"
        R"(}})"sv,
        timestamp.count());
    std::string body{std::data(encode_buffer_), std::size(encode_buffer_)};
    log::debug(R"(body="{}")"sv, body);
    web::rest::Request request{
        .method = web::http::Method::GET,
        .path = "/api/v3/openOrders"sv,
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

void OrderEntry::get_open_orders_ack(Trace<web::rest::Response> const &event) {
  profile_.open_orders_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::OpenOrders open_orders{body, decode_buffer_};
      Trace event_2{event, open_orders};
      (*this)(event_2);
      request_.respond_orders = clock::get_system();  // completion
      download_orders_ = false;
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      download_orders_ = false;
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntry::operator()(Trace<json::OpenOrders> const &event) {
  auto &[trace_info, open_orders] = event;
  for (auto &order : open_orders.data) {
    log::info<2>("order={}"sv, order);
    if (std::empty(order.client_order_id))
      continue;
    open_orders_symbols_.emplace(order.symbol);
    auto side = json::map(order.side);
    auto order_type = json::map(order.type);
    auto time_in_force = json::map(order.time_in_force);
    auto external_order_id = fmt::format("{}"sv, order.order_id);  // alloc
    auto order_status = json::map(order.status);
    oms::OrderUpdate order_update{
        .account = security_.get_account(),
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
}

// ...

void OrderEntry::refresh_listen_key(std::chrono::nanoseconds now) {
  if (!ready_)
    return;
  if (listen_key_refresh_ == listen_key_refresh_.zero() || now < listen_key_refresh_)
    return;
  log::info<1>("Refreshing listen key..."sv);
  listen_key_refresh_ = now + Flags::rest_listen_key_refresh();
  get_listen_key();
}

// new-order

void OrderEntry::new_order(
    Event<CreateOrder> const &event, oms::Order const &order, std::string_view const &request_id) {
  profile_.new_order([&]() {
    if (!ready())
      throw oms::NotReady{"not ready"sv};
    auto &[message_info, create_order] = event;
    open_orders_symbols_.emplace(create_order.symbol);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
    auto body = json::new_order(encode_buffer_, create_order, order, request_id, recv_window);
    log::debug(R"(body="{}")"sv, body);
    auto query = security_.create_query(body);
    auto headers = security_.create_headers();
    web::rest::Request request{
        .method = web::http::Method::POST,
        .path = "/api/v3/order"sv,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
        .headers = headers,
        .body = body,
        .quality_of_service = io::QualityOfService::IMMEDIATE,
    };
    auto callback = [this, user_id = message_info.source, order_id = create_order.order_id](
                        [[maybe_unused]] auto &request_id, auto &response) {
      auto version = uint32_t{1};
      TraceInfo trace_info;
      Trace event{trace_info, response};
      new_order_ack(event, user_id, order_id, version);
    };
    (*connection_)(request_id, request, callback);
  });
}

void OrderEntry::new_order_ack(
    Trace<web::rest::Response> const &event, uint8_t user_id, uint32_t order_id, uint32_t version) {
  profile_.new_order_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::NewOrder new_order{body, decode_buffer_};
      log::debug("new_order={}"sv, new_order);
      Trace event_2{event, new_order};
      (*this)(event_2, user_id, order_id, version);
    };
    auto handle_error = [&](auto origin, auto status, auto error, auto text) {
      oms::Response response{
          .type = RequestType::CREATE_ORDER,
          .origin = origin,
          .status = status,
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

void OrderEntry::operator()(Trace<json::NewOrder> const &event, uint8_t user_id, uint32_t order_id, uint32_t version) {
  auto &[trace_info, new_order] = event;
  log::info<2>("new_order={}, user_id={}, order_id={}, version={}"sv, new_order, user_id, order_id, version);
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
  oms::Response response{
      .type = RequestType::CREATE_ORDER,
      .origin = Origin::EXCHANGE,
      .status = RequestStatus::ACCEPTED,
      .error = {},
      .text = {},
      .version = version,
      .request_id = {},
      .quantity = NaN,
      .price = NaN,
  };
  oms::OrderUpdate order_update{
      .account = security_.get_account(),
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
      .update_time_utc = utils::safe_cast(new_order.transact_time),
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
  (*this)(event_2, user_id, order_id, order_update);
}

// cancel-replace-order

void OrderEntry::cancel_replace_order(
    HoldCancelOrder const &hold_cancel_order,
    Event<CreateOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id) {
  profile_.cancel_replace_order([&]() {
    if (!ready())
      throw oms::NotReady{"not ready"sv};
    auto &[message_info, create_order] = event;
    auto body = json::cancel_replace_order(
        encode_buffer_,
        hold_cancel_order.request_id,
        hold_cancel_order.previous_request_id,
        create_order,
        order,
        request_id,
        utils::safe_cast(Flags::rest_order_recv_window()),
        flags::Flags::cancel_replace_stop_on_failure());
    log::info(R"(DEBUG body="{}")"sv, body);
    auto query = security_.create_query(body);
    auto headers = security_.create_headers();
    web::rest::Request request{
        .method = web::http::Method::POST,
        .path = "/api/v3/order/cancelReplace"sv,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
        .headers = headers,
        .body = body,
        .quality_of_service = io::QualityOfService::IMMEDIATE,
    };
    auto callback = [this,
                     user_id = message_info.source,
                     cancel_order_id = hold_cancel_order.cancel_order.order_id,
                     cancel_version = hold_cancel_order.cancel_order.version,
                     create_order_id = create_order.order_id]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      cancel_replace_order_ack(event, user_id, cancel_order_id, cancel_version, create_order_id, uint32_t{1});
    };
    (*connection_)(request_id, request, callback);
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
void OrderEntry::cancel_replace_order_ack(
    Trace<web::rest::Response> const &event,
    uint8_t user_id,
    uint32_t cancel_order_id,
    uint32_t cancel_version,
    uint32_t create_order_id,
    uint32_t create_version) {
  profile_.cancel_replace_order_ack([&]() {
    auto &trace_info = event.trace_info;
    auto &response = event.value;
    log::info(
        "DEBUG user_id={}, cancel_order_id={}, cancel_version={}, create_order_id={}, create_version={}"sv,
        user_id,
        cancel_order_id,
        cancel_version,
        create_order_id,
        create_version);
    try {
      auto [status, category, body_] = response.result();
      auto body = body_;  // XXX clang workaround
      log::info(R"(DEBUG status={}, category={}, body="{}")"sv, status, category, body);
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
          dispatch_error_2(category, status, parse, [&]([[maybe_unused]] auto status, auto error, auto text) {
            {  // cancel
              oms::Response response{
                  .type = RequestType::CANCEL_ORDER,
                  .origin = Origin::EXCHANGE,
                  .status = status,
                  .error = error,
                  .text = text,
                  .version = cancel_version,
                  .request_id = {},
                  .quantity = NaN,
                  .price = NaN,
              };
              if (shared_.update_order(
                      user_id, cancel_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {
                      })) {
              } else {
                log::warn(
                    "Did not find order: user_id={}, order_id={}, version={}"sv,
                    user_id,
                    cancel_order_id,
                    cancel_version);
              }
            }
            {  // create
              oms::Response response{
                  .type = RequestType::CREATE_ORDER,
                  .origin = Origin::EXCHANGE,
                  .status = status,
                  .error = error,
                  .text = text,
                  .version = create_version,
                  .request_id = {},
                  .quantity = NaN,
                  .price = NaN,
              };
              if (shared_.update_order(
                      user_id, create_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {
                      })) {
              } else {
                log::warn(
                    "Did not find order: user_id={}, order_id={}, version={}"sv,
                    user_id,
                    create_order_id,
                    create_version);
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
        oms::Response response{
            .type = RequestType::CANCEL_ORDER,
            .origin = Origin::GATEWAY,
            .status = e.request_status(),
            .error = e.error(),
            .text = e.what(),
            .version = cancel_version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        if (shared_.update_order(
                user_id, cancel_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
        } else {
          log::warn(
              "Did not find order: user_id={}, order_id={}, version={}"sv, user_id, cancel_order_id, cancel_version);
        }
      }
      {  // create
        oms::Response response{
            .type = RequestType::CREATE_ORDER,
            .origin = Origin::GATEWAY,
            .status = e.request_status(),
            .error = e.error(),
            .text = e.what(),
            .version = create_version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        if (shared_.update_order(
                user_id, create_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
        } else {
          log::warn(
              "Did not find order: user_id={}, order_id={}, version={}"sv, user_id, create_order_id, create_version);
        }
      }
    }
  });
}

void OrderEntry::operator()(
    Trace<json::CancelReplaceOrder> const &event,
    uint8_t user_id,
    uint32_t cancel_order_id,
    uint32_t cancel_version,
    uint32_t create_order_id,
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
    case UNDEFINED:
    case UNKNOWN:
      log::warn("Unexpected"sv);
      break;
    case SUCCESS: {
      auto &cancel_order = cancel_replace_order.cancel_response;
      auto side = json::map(cancel_order.side);
      auto order_type = json::map(cancel_order.type);
      auto time_in_force = json::map(cancel_order.time_in_force);
      auto external_order_id = fmt::format("{}"sv, cancel_order.order_id);  // alloc
      auto order_status = json::map(cancel_order.status);
      oms::Response response{
          .type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = cancel_version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      oms::OrderUpdate order_update{
          .account = security_.get_account(),
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
      if (shared_.update_order(
              user_id,
              cancel_order_id,
              stream_id_,
              trace_info,
              response,
              order_update,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(
            "Did not find order: user_id={}, order_id={}, version={}"sv, user_id, cancel_order_id, cancel_version);
      }
      break;
    }
    case FAILURE:
    case NOT_ATTEMPTED: {
      auto &cancel_order = cancel_replace_order.cancel_response;
      oms::Response response{
          .type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .status = RequestStatus::REJECTED,
          .error = json::guess_error(cancel_order.code),
          .text = cancel_order.msg,
          .version = cancel_version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              user_id, cancel_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(
            "Did not find order: user_id={}, order_id={}, version={}"sv, user_id, cancel_order_id, cancel_version);
      }
      break;
    }
  }
  switch (cancel_replace_order.cancel_result) {
    using enum json::SuccessOrFailure::type_t;
    case UNDEFINED:
    case UNKNOWN:
      log::warn("Unexpected"sv);
      break;
    case SUCCESS: {
      auto &new_order = cancel_replace_order.new_order_response;
      auto side = json::map(new_order.side);
      auto order_type = json::map(new_order.type);
      auto time_in_force = json::map(new_order.time_in_force);
      auto external_order_id = fmt::format("{}"sv, new_order.order_id);  // alloc
      auto order_status = json::map(new_order.status);
      oms::Response response{
          .type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .status = RequestStatus::ACCEPTED,
          .error = {},
          .text = {},
          .version = create_version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      oms::OrderUpdate order_update{
          .account = security_.get_account(),
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
      };
      if (shared_.update_order(
              user_id,
              create_order_id,
              stream_id_,
              trace_info,
              response,
              order_update,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(
            "Did not find order: user_id={}, order_id={}, version={}"sv, user_id, create_order_id, create_version);
      }
      break;
    }
    case FAILURE:
    case NOT_ATTEMPTED: {
      auto &new_order = cancel_replace_order.new_order_response;
      oms::Response response{
          .type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .status = RequestStatus::REJECTED,
          .error = json::guess_error(new_order.code),
          .text = new_order.msg,
          .version = create_version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              user_id, create_order_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(
            "Did not find order: user_id={}, order_id={}, version={}"sv, user_id, create_order_id, create_version);
      }
      break;
    }
  }
}

void OrderEntry::operator()(
    Trace<json::CancelReplaceOrderError> const &event,
    uint8_t user_id,
    uint32_t cancel_order_id,
    uint32_t cancel_version,
    uint32_t create_order_id,
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

void OrderEntry::cancel_order(
    Event<CancelOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  profile_.cancel_order([&]() {
    if (!ready())
      throw oms::NotReady{"not ready"sv};
    auto &[message_info, cancel_order] = event;
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
    auto body = json::cancel_order(encode_buffer_, cancel_order, order, request_id, previous_request_id, recv_window);
    log::debug(R"(body="{}")"sv, body);
    auto query = security_.create_query(body);
    auto headers = security_.create_headers();
    web::rest::Request request{
        .method = web::http::Method::DELETE,
        .path = "/api/v3/order"sv,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
        .headers = headers,
        .body = body,
        .quality_of_service = io::QualityOfService::IMMEDIATE,
    };
    auto callback =
        [this, user_id = message_info.source, order_id = cancel_order.order_id, version = cancel_order.version](
            [[maybe_unused]] auto &request_id, auto &response) {
          TraceInfo trace_info;
          Trace event{trace_info, response};
          cancel_order_ack(event, user_id, order_id, version);
        };
    (*connection_)(request_id, request, callback);
  });
}

void OrderEntry::cancel_order_ack(
    Trace<web::rest::Response> const &event, uint8_t user_id, uint32_t order_id, uint32_t version) {
  profile_.cancel_order_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::CancelOrder cancel_order{body};
      log::debug("cancel_order={}"sv, cancel_order);
      Trace event_2{event, cancel_order};
      (*this)(event_2, user_id, order_id, version);
    };
    auto handle_error = [&](auto origin, auto status, auto error, auto text) {
      oms::Response response{
          .type = RequestType::CANCEL_ORDER,
          .origin = origin,
          .status = status,
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

void OrderEntry::operator()(
    Trace<json::CancelOrder> const &event, uint8_t user_id, uint32_t order_id, uint32_t version) {
  auto &[trace_info, cancel_order] = event;
  log::info<2>("cancel_order={}, user_id={}, order_id={}, version={}"sv, cancel_order, user_id, order_id, version);
  auto side = json::map(cancel_order.side);
  auto order_type = json::map(cancel_order.type);
  auto time_in_force = json::map(cancel_order.time_in_force);
  auto external_order_id = fmt::format("{}"sv, cancel_order.order_id);  // alloc
  auto order_status = json::map(cancel_order.status);
  oms::Response response{
      .type = RequestType::CANCEL_ORDER,
      .origin = Origin::EXCHANGE,
      .status = RequestStatus::ACCEPTED,
      .error = {},
      .text = {},
      .version = version,
      .request_id = {},
      .quantity = NaN,
      .price = NaN,
  };
  oms::OrderUpdate order_update{
      .account = security_.get_account(),
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
  (*this)(event_2, user_id, order_id, order_update);
}

void OrderEntry::cancel_all_open_orders(
    Event<CancelAllOrders> const &, [[maybe_unused]] std::string_view const &request_id) {
  profile_.cancel_all_open_orders([&]() {
    if (!ready()) {
      log::warn("*** NOT POSSIBLE TO CANCEL ALL OPEN ORDERS (NOT READY) ***"sv);
      return;
    }
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
    for (auto &symbol : open_orders_symbols_) {
      auto body = json::cancel_all_open_orders(encode_buffer_, symbol, recv_window);
      log::debug(R"(body="{}")"sv, body);
      auto query = security_.create_query(body);
      auto headers = security_.create_headers();
      web::rest::Request request{
          .method = web::http::Method::DELETE,
          .path = "/api/v3/openOrders"sv,
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
        cancel_all_open_orders_ack(event);
      };
      (*connection_)(request_id, request, callback);
    }
  });
}

void OrderEntry::cancel_all_open_orders_ack(Trace<web::rest::Response> const &event) {
  profile_.cancel_all_open_orders_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::CancelAllOpenOrders cancel_all_open_orders{body, decode_buffer_};
      log::debug("cancel_all_open_orders={}"sv, cancel_all_open_orders);
      Trace event_2{event, cancel_all_open_orders};
      (*this)(event_2);
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      switch (error) {
        using enum Error;
        case TOO_LATE_TO_MODIFY_OR_CANCEL:
          break;
        default:
          log::warn(R"(error={}, text="{}")"sv, error, text);
      }
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntry::operator()(Trace<json::CancelAllOpenOrders> const &event) {
  auto &[trace_info, cancel_all_open_orders] = event;
  log::info<2>("cancel_all_open_orders={}"sv, cancel_all_open_orders);
  for (auto &order : cancel_all_open_orders.data) {
    log::debug("order={}"sv, order);
    if (std::empty(order.client_order_id))
      continue;
    auto side = json::map(order.side);
    auto order_type = json::map(order.type);
    auto time_in_force = json::map(order.time_in_force);
    auto external_order_id = fmt::format("{}"sv, order.order_id);  // alloc
    auto order_status = json::map(order.status);
    oms::OrderUpdate order_update{
        .account = security_.get_account(),
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

template <typename SuccessHandler, typename ErrorHandler>
void OrderEntry::process_response(
    web::rest::Response const &response, SuccessHandler success_handler, ErrorHandler error_handler) {
  try {
    auto [status, category, body] = response.result();
    log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
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
          case I_AM_A_TEAPOT:      // 418
          case TOO_MANY_REQUESTS:  // 429
            error_handler(
                Origin::EXCHANGE,
                RequestStatus::REJECTED,
                Error::REQUEST_RATE_LIMIT_REACHED,
                magic_enum::enum_name(status));
            break;
          case CONFLICT:  // 409
            assert(false);
            [[fallthrough]];
          default: {
            json::Error error{body};
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, json::guess_error(error.code), error.msg);
          }
        }
        break;
      case SERVER_ERROR:  // 5xx
        error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, magic_enum::enum_name(status));
        break;
      default:
        response.expect(web::http::Status::OK);  // throws
    }
  } catch (oms::Exception &e) {
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
void OrderEntry::operator()(Trace<oms::Response> const &event, uint8_t user_id, uint32_t order_id, Args &&...args) {
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

template <typename... Args>
void OrderEntry::operator()(
    Trace<oms::OrderUpdate> const &event, std::string_view const &client_order_id, Args &&...args) {
  auto &[trace_info, order_update] = event;
  if (shared_.update_order(
          client_order_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

// note! used by cancel-replace
template <typename Parse, typename Callback>
void OrderEntry::dispatch_error_2(
    web::http::Category category, web::http::Status status, Parse parse, Callback callback) {
  switch (category) {
    using enum web::http::Category;
    case UNKNOWN:
      break;
    case INFORMATIONAL_RESPONSE:
    case SUCCESS:
    case REDIRECTION:
      assert(false);
      break;
    case CLIENT_ERROR:
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
          case FORBIDDEN:          // 403
          case I_AM_A_TEAPOT:      // 418
          case TOO_MANY_REQUESTS:  // 429
            callback(RequestStatus::REJECTED, Error::REQUEST_RATE_LIMIT_REACHED, magic_enum::enum_name(status));
            break;
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
    case SERVER_ERROR:
      // HTTP 5XX return codes are used for internal errors; the issue is on Binance's side.
      //   It is important to NOT treat this as a failure operation; the execution status is UNKNOWN
      //   and could have been a success.
      callback(RequestStatus::ERROR, Error::UNKNOWN, magic_enum::enum_name(status));
      break;
  }
}

void OrderEntry::test(web::http::Status status) {
  if (status != web::http::Status::FORBIDDEN) [[likely]]
    return;
  waf_limit_violation();
}

void OrderEntry::waf_limit_violation() {
  if (Flags::rest_terminate_on_403()) {
    log::fatal("WAF limit violation"sv);
  } else {
    log::warn("WAF limit violation"sv);
    (*connection_).suspend(Flags::rest_back_off_delay());
  }
}

}  // namespace binance
}  // namespace roq
