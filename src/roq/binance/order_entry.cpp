/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/order_entry.h"

#include <utility>

#include "roq/utils/mask.h"
#include "roq/utils/number.h"
#include "roq/utils/safe_cast.h"
#include "roq/utils/update.h"

#include "roq/core/back_emplacer.h"
#include "roq/core/charconv.h"

#include "roq/core/metrics/factory.h"

#include "roq/binance/flags.h"

#include "roq/binance/json/error.h"
#include "roq/binance/json/utils.h"

using namespace std::literals;

namespace roq {
namespace binance {

namespace {
const auto NAME = "om"sv;
const auto SUPPORTS = utils::Mask{
    SupportType::CREATE_ORDER,
    SupportType::CANCEL_ORDER,
    SupportType::ORDER_ACK,
    SupportType::ORDER,
    SupportType::TRADE,
    SupportType::FUNDS,
};

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};

auto create_connection(auto &handler, auto &context) {
  core::web::Client::Config config{
      .decode_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
      .validate_certificate = server::Flags::tls_validate_certificate(),
      .uri = Flags::rest_uri(),
      .proxy = Flags::rest_proxy(),
      .user_agent = ROQ_PACKAGE_NAME,
      .connection = core::http::Connection::KEEP_ALIVE,
      .allow_pipelining = true,
      .request_timeout = Flags::rest_request_timeout(),
      .ping_frequency = Flags::rest_ping_freq(),
      .ping_path = Flags::rest_ping_path(),
  };
  return core::web::Client{handler, context, config};
}
}  // namespace

OrderEntry::OrderEntry(
    Handler &handler,
    core::io::Context &context,
    uint16_t stream_id,
    Security &security,
    Shared &shared)
    : handler_(handler), stream_id_(stream_id),
      name_(fmt::format("{}:{}:{}"sv, stream_id_, NAME, security.get_account())),
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
          .cancel_order = create_metrics(name_, "cancel_order"sv),
          .cancel_order_ack = create_metrics(name_, "cancel_order_ack"sv),
          .cancel_all_open_orders = create_metrics(name_, "cancel_all_open_orders"sv),
          .cancel_all_open_orders_ack = create_metrics(name_, "cancel_all_open_orders_ack"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
      },
      security_(security), shared_(shared),
      download_(Flags::rest_request_timeout(), [this](auto state) { return download(state); }) {
}

void OrderEntry::operator()(const Event<Start> &) {
  connection_.start();
}

void OrderEntry::operator()(const Event<Stop> &) {
  connection_.stop();
}

void OrderEntry::operator()(const Event<Timer> &event) {
  connection_.refresh(event.value.now);
  refresh_listen_key();
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
      .write(profile_.cancel_order, metrics::PROFILE)
      .write(profile_.cancel_order_ack, metrics::PROFILE)
      .write(profile_.cancel_all_open_orders, metrics::PROFILE)
      .write(profile_.cancel_all_open_orders_ack, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY);
}

uint16_t OrderEntry::operator()(
    const Event<CreateOrder> &event, const oms::Order &order, const std::string_view &request_id) {
  new_order(event, order, request_id);
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    const Event<ModifyOrder> &,
    const oms::Order &,
    [[maybe_unused]] const std::string_view &request_id,
    [[maybe_unused]] const std::string_view &previous_request_id) {
  throw oms::NotSupported("not supported"sv);
}

uint16_t OrderEntry::operator()(
    const Event<CancelOrder> &event,
    const oms::Order &order,
    const std::string_view &request_id,
    const std::string_view &previous_request_id) {
  cancel_order(event, order, request_id, previous_request_id);
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    const Event<CancelAllOrders> &event, const std::string_view &request_id) {
  cancel_all_open_orders(event, request_id);
  return stream_id_;
}

void OrderEntry::operator()(const core::web::Client::Connected &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void OrderEntry::operator()(const core::web::Client::Disconnected &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading())
    download_.reset();
}

void OrderEntry::operator()(const core::web::Client::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .account = security_.get_account(),
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntry::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = security_.get_account(),
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::REST,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"sv, stream_status);
    server::create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t OrderEntry::download(OrderEntryState state) {
  switch (state) {
    case OrderEntryState::UNDEFINED:
      assert(false);
      break;
    case OrderEntryState::LISTEN_KEY:
      get_listen_key();
      return 1;
    case OrderEntryState::ACCOUNT:
      get_account();
      return 1;
    case OrderEntryState::OPEN_ORDERS:
      get_open_orders();
      return 1;
    case OrderEntryState::DONE:
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
    auto method = core::http::Method::POST;
    auto path = "/api/v3/userDataStream"sv;
    auto headers = security_.create_headers();
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "listen_key"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_listen_key_ack(event, sequence);
        });
  });
}

void OrderEntry::get_listen_key_ack(
    const server::Trace<core::web::Response> &event, [[maybe_unused]] uint32_t sequence) {
  profile_.listen_key_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = OrderEntryState::LISTEN_KEY;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      response.expect(core::http::Status::OK);
      auto listen_key = core::json::Parser::create<json::ListenKey>(body);
      server::Trace event(trace_info, listen_key);
      (*this)(event);
      download_.check_relaxed(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      if (download_.downloading())
        download_.retry(state);
    }
  });
}

void OrderEntry::operator()(const server::Trace<json::ListenKey> &event) {
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
  auto now = core::clock::GetSystem();
  listen_key_refresh_ = now + Flags::rest_listen_key_refresh();
}

// account

void OrderEntry::get_account() {
  profile_.account([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v3/account"sv;
    auto query = security_.create_query();
    auto headers = security_.create_headers();
    core::web::Request request{
        .method = method,
        .path = path,
        .query = query,
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "account"sv, request, [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_account_ack(event, sequence);
        });
  });
}

void OrderEntry::get_account_ack(
    const server::Trace<core::web::Response> &event, uint32_t sequence) {
  profile_.account_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = OrderEntryState::ACCOUNT;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto account = core::json::Parser::create<json::Account>(body, buffer);
      server::Trace event(trace_info, account);
      (*this)(event);
      download_.check(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void OrderEntry::operator()(const server::Trace<json::Account> &event) {
  auto &[trace_info, account] = event;
  log::info<2>("account={}"sv, account);
  for (auto &item : account.balances) {
    log::debug("item={}"sv, item);
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
    auto method = core::http::Method::GET;
    auto path = "/api/v3/openOrders"sv;
    auto query = security_.create_query();
    auto headers = security_.create_headers();
    auto timestamp = core::clock::GetRealTime<std::chrono::milliseconds>();
    auto body = fmt::format(
        R"({{)"
        R"("timestamp":{})"
        R"(}})"sv,
        timestamp.count());
    log::debug(R"(body="{}")"sv, body);
    core::web::Request request{
        .method = method,
        .path = path,
        .query = query,
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "open_orders"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_open_orders_ack(event, sequence);
        });
  });
}

void OrderEntry::get_open_orders_ack(
    const server::Trace<core::web::Response> &event, uint32_t sequence) {
  profile_.open_orders_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = OrderEntryState::OPEN_ORDERS;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto open_orders = core::json::Parser::create<json::OpenOrders>(body, buffer);
      server::Trace event(trace_info, open_orders);
      (*this)(event);
      download_.check(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void OrderEntry::operator()(const server::Trace<json::OpenOrders> &event) {
  auto &[trace_info, open_orders] = event;
  for (auto &order : open_orders.data) {
    log::info<2>("order={}"sv, order);
    if (std::empty(order.client_order_id))
      continue;
    open_orders_symbols_.emplace(order.symbol);
    auto side = json::map(order.side);
    auto order_type = json::map(order.type);
    auto time_in_force = json::map(order.time_in_force);
    auto external_order_id = fmt::format("{}"sv, order.order_id);
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
        .execution_instruction = {},
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
    };
    shared_.create_order(order.client_order_id, stream_id_, trace_info, order_update);
  }
}

// ...

void OrderEntry::refresh_listen_key() {
  if (!ready_)
    return;
  auto now = core::clock::GetSystem();
  if (listen_key_refresh_ == listen_key_refresh_.zero() || now < listen_key_refresh_)
    return;
  log::info<1>("Refreshing listen key..."sv);
  listen_key_refresh_ = now + Flags::rest_listen_key_refresh();
  get_listen_key();
}

// new-order

void OrderEntry::new_order(
    const Event<CreateOrder> &event, const oms::Order &order, const std::string_view &request_id) {
  profile_.new_order([&]() {
    if (!ready())
      throw oms::NotReady("not ready"sv);
    auto &[message_info, create_order] = event;
    open_orders_symbols_.emplace(create_order.symbol);
    auto method = core::http::Method::POST;
    auto path = "/api/v3/order"sv;
    auto side = json::map(create_order.side).as_raw_text();
    auto type = json::map(create_order.order_type).as_raw_text();
    auto time_in_force = json::map(create_order.time_in_force).as_raw_text();
    auto recv_window =
        std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
    std::string body;
    if (std::isnan(create_order.stop_price)) {
      body = fmt::format(
          R"(symbol={}&)"
          R"(side={}&)"
          R"(type={}&)"
          R"(timeInForce={}&)"
          R"(quantity={}&)"
          R"(price={}&)"
          R"(newClientOrderId={}&)"
          R"(recvWindow={})"sv,
          create_order.symbol,
          side,
          type,
          time_in_force,
          utils::Number{create_order.quantity, order.quantity_decimals},
          utils::Number{create_order.price, order.price_decimals},
          request_id,
          recv_window.count());
    } else {
      body = fmt::format(
          R"(symbol={}&)"
          R"(side={}&)"
          R"(type={}&)"
          R"(timeInForce={}&)"
          R"(quantity={}&)"
          R"(price={}&)"
          R"(newClientOrderId={}&)"
          R"(stopPrice={}&)"
          R"(recvWindow={})"
          R"(}})"sv,
          create_order.symbol,
          side,
          type,
          time_in_force,
          utils::Number{create_order.quantity, order.quantity_decimals},
          utils::Number{create_order.price, order.price_decimals},
          request_id,
          utils::Number{create_order.stop_price, order.price_decimals},
          recv_window.count());
    }
    log::debug(R"(body="{}")"sv, body);
    auto query = security_.create_query(body);
    auto headers = security_.create_headers();
    core::web::Request request{
        .method = method,
        .path = path,
        .query = query,
        .accept = core::http::Accept::JSON,
        .content_type = core::http::ContentType::FORM,
        .headers = headers,
        .body = body,
        .quality_of_service = core::web::QualityOfService::IMMEDIATE,
    };
    connection_(
        request_id,
        request,
        [this, user_id = message_info.source, order_id = create_order.order_id](
            [[maybe_unused]] auto &request_id, auto &response) {
          const uint32_t version = 1;
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          new_order_ack(event, user_id, order_id, version);
        });
  });
}

void OrderEntry::new_order_ack(
    const server::Trace<core::web::Response> &event,
    uint8_t user_id,
    uint32_t order_id,
    uint32_t version) {
  profile_.new_order_ack([&]() {
    auto &[trace_info, response] = event;
    log::debug("user_id={}, order_id={}, version={}"sv, user_id, order_id, version);
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      switch (category) {
        case core::http::Category::SUCCESS: {  // 2xx
          core::json::Buffer buffer(decode_buffer_);
          auto new_order = core::json::Parser::create<json::NewOrder>(body, buffer);
          server::Trace event(trace_info, new_order);
          (*this)(event, user_id, order_id, version);
          break;
        }
        case core::http::Category::CLIENT_ERROR: {  // 4xx
          auto error = core::json::Parser::create<json::Error>(body);
          oms::Response response{
              .type = RequestType::CREATE_ORDER,
              .origin = Origin::EXCHANGE,
              .status = RequestStatus::REJECTED,
              .error = json::guess_error(error.code),
              .text = error.msg,
              .version = version,
              .request_id = {},
              .quantity = NaN,
              .price = NaN,
          };
          if (shared_.update_order(
                  user_id,
                  order_id,
                  stream_id_,
                  trace_info,
                  response,
                  []([[maybe_unused]] auto &order) {})) {
          } else {
            log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
          }
          break;
        }
        default:
          response.expect(core::http::Status::OK);  // throws
      }
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      oms::Response response{
          .type = RequestType::CREATE_ORDER,
          .origin = Origin::GATEWAY,
          .status = e.request_status(),
          .error = e.error(),
          .text = e.what(),
          .version = version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              user_id,
              order_id,
              stream_id_,
              trace_info,
              response,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
      }
    }
  });
}

void OrderEntry::operator()(
    const server::Trace<json::NewOrder> &event,
    uint8_t user_id,
    uint32_t order_id,
    uint32_t version) {
  auto &[trace_info, new_order] = event;
  log::info<2>(
      "new_order={}, user_id={}, order_id={}, version={}"sv, new_order, user_id, order_id, version);
  auto side = json::map(new_order.side);
  auto order_type = json::map(new_order.type);
  auto time_in_force = json::map(new_order.time_in_force);
  auto external_order_id = fmt::format("{}"sv, new_order.order_id);
  auto order_status = json::map(new_order.status);
  auto remaining_quantity = new_order.orig_qty - new_order.executed_qty;
  auto average_traded_price = utils::compare(new_order.executed_qty, 0.0) == 0
                                  ? NaN
                                  : (new_order.cummulative_quote_qty / new_order.executed_qty);
  auto last_traded_quantity = NaN;  // note! could also use new_order.executed_qty
  auto last_traded_price = NaN;     // note! could also use average_traded_price
  double tmp = 0.0;
  for (auto &item : new_order.fills) {
    last_traded_quantity += item.qty;
    tmp += item.price * item.qty;
  }
  if (utils::compare(last_traded_quantity, 0.0) > 0)
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
      .execution_instruction = {},
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
  };
  if (shared_.update_order(
          user_id,
          order_id,
          stream_id_,
          trace_info,
          response,
          order_update,
          [&]([[maybe_unused]] auto &order) {
            // note! fills will be reported by drop copy
          })) {
  } else {
    log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
  }
}

// cancel-order

void OrderEntry::cancel_order(
    const Event<CancelOrder> &event,
    const oms::Order &order,
    const std::string_view &request_id,
    [[maybe_unused]] const std::string_view &previous_request_id) {
  profile_.cancel_order([&]() {
    if (!ready())
      throw oms::NotReady("not ready"sv);
    auto &[message_info, cancel_order] = event;
    auto method = core::http::Method::DELETE;
    auto path = "/api/v3/order"sv;
    auto recv_window =
        std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
    auto body = fmt::format(
        R"(symbol={}&)"
        R"(origClientOrderId={}&)"
        R"(newClientOrderId={}&)"
        R"(recvWindow={})"sv,
        order.symbol,
        previous_request_id,
        request_id,
        recv_window.count());
    log::debug(R"(body="{}")"sv, body);
    auto query = security_.create_query(body);
    auto headers = security_.create_headers();
    core::web::Request request{
        .method = method,
        .path = path,
        .query = query,
        .accept = core::http::Accept::JSON,
        .content_type = core::http::ContentType::FORM,
        .headers = headers,
        .body = body,
        .quality_of_service = core::web::QualityOfService::IMMEDIATE,
    };
    connection_(
        request_id,
        request,
        [this,
         user_id = message_info.source,
         order_id = cancel_order.order_id,
         version = cancel_order.version]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          cancel_order_ack(event, user_id, order_id, version);
        });
  });
}

void OrderEntry::cancel_order_ack(
    const server::Trace<core::web::Response> &event,
    uint8_t user_id,
    uint32_t order_id,
    uint32_t version) {
  profile_.cancel_order_ack([&]() {
    auto &[trace_info, response] = event;
    log::debug("user_id={}, order_id={}, version={}"sv, user_id, order_id, version);
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      switch (category) {
        case core::http::Category::SUCCESS: {  // 2xx
          auto cancel_order = core::json::Parser::create<json::CancelOrder>(body);
          server::Trace event(trace_info, cancel_order);
          (*this)(event, user_id, order_id, version);
          break;
        }
        case core::http::Category::CLIENT_ERROR: {  // 4xx
          auto error = core::json::Parser::create<json::Error>(body);
          auto error_2 = json::guess_error(error.code);
          oms::Response response{
              .type = RequestType::CANCEL_ORDER,
              .origin = Origin::EXCHANGE,
              .status = RequestStatus::REJECTED,
              .error = error_2,
              .text = error.msg,
              .version = version,
              .request_id = {},
              .quantity = NaN,
              .price = NaN,
          };
          if (shared_.update_order(
                  user_id,
                  order_id,
                  stream_id_,
                  trace_info,
                  response,
                  []([[maybe_unused]] auto &order) {})) {
          } else {
            log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
          }
          break;
        }
        default:
          response.expect(core::http::Status::OK);  // throws
      }
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      oms::Response response{
          .type = RequestType::CANCEL_ORDER,
          .origin = Origin::GATEWAY,
          .status = e.request_status(),
          .error = e.error(),
          .text = e.what(),
          .version = version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              user_id,
              order_id,
              stream_id_,
              trace_info,
              response,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(
            "Did not find order: user_id={}, order_id={}, version={}"sv,
            user_id,
            order_id,
            version);
      }
    }
  });
}

void OrderEntry::operator()(
    const server::Trace<json::CancelOrder> &event,
    uint8_t user_id,
    uint32_t order_id,
    uint32_t version) {
  auto &[trace_info, cancel_order] = event;
  log::info<2>(
      "cancel_order={}, user_id={}, order_id={}, version={}"sv,
      cancel_order,
      user_id,
      order_id,
      version);
  auto side = json::map(cancel_order.side);
  auto order_type = json::map(cancel_order.type);
  auto time_in_force = json::map(cancel_order.time_in_force);
  auto external_order_id = fmt::format("{}"sv, cancel_order.order_id);
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
      .execution_instruction = {},
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
  };
  if (shared_.update_order(
          user_id,
          order_id,
          stream_id_,
          trace_info,
          response,
          order_update,
          []([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
  }
}

void OrderEntry::cancel_all_open_orders(
    const Event<CancelAllOrders> &, [[maybe_unused]] const std::string_view &request_id) {
  profile_.cancel_all_open_orders([&]() {
    for (auto &symbol : open_orders_symbols_) {
      auto method = core::http::Method::DELETE;
      auto path = "/api/v3/openOrders"sv;
      auto recv_window =
          std::chrono::duration_cast<std::chrono::milliseconds>(Flags::rest_order_recv_window());
      auto body = fmt::format(
          R"(symbol={}&)"
          R"(recvWindow={})"sv,
          symbol,
          recv_window.count());
      log::debug(R"(body="{}")"sv, body);
      auto query = security_.create_query(body);
      auto headers = security_.create_headers();
      core::web::Request request{
          .method = method,
          .path = path,
          .query = query,
          .accept = core::http::Accept::JSON,
          .content_type = core::http::ContentType::FORM,
          .headers = headers,
          .body = body,
          .quality_of_service = core::web::QualityOfService::IMMEDIATE,
      };
      connection_(request_id, request, [this]([[maybe_unused]] auto &request_id, auto &response) {
        auto trace_info = server::create_trace_info();
        server::Trace event(trace_info, response);
        cancel_all_open_orders_ack(event);
      });
    }
  });
}

void OrderEntry::cancel_all_open_orders_ack(const server::Trace<core::web::Response> &event) {
  profile_.cancel_all_open_orders_ack([&]() {
    auto &[trace_info, response] = event;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      switch (category) {
        case core::http::Category::SUCCESS: {  // 2xx
          core::json::Buffer buffer(decode_buffer_);
          auto cancel_order = core::json::Parser::create<json::CancelAllOpenOrders>(body, buffer);
          server::Trace event(trace_info, cancel_order);
          (*this)(event);
          break;
        }
        case core::http::Category::CLIENT_ERROR: {  // 4xx
          auto error = core::json::Parser::create<json::Error>(body);
          log::warn("error={}"sv, error);
          // XXX HANS ???
          // not outstanding orders: {code=-2011, msg="Unknown order sent."}
          break;
        }
        default:
          response.expect(core::http::Status::OK);  // throws
      }
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      // XXX HANS ???
    }
  });
}

void OrderEntry::operator()(const server::Trace<json::CancelAllOpenOrders> &event) {
  auto &[trace_info, cancel_all_open_orders] = event;
  log::info<2>("cancel_all_open_orders={}"sv, cancel_all_open_orders);
  for (auto &order : cancel_all_open_orders.data) {
    log::debug("order={}"sv, order);
    if (std::empty(order.client_order_id))
      continue;
    auto side = json::map(order.side);
    auto order_type = json::map(order.type);
    auto time_in_force = json::map(order.time_in_force);
    auto external_order_id = fmt::format("{}"sv, order.order_id);
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
        .execution_instruction = {},
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
    };
    shared_.update_order(
        order.client_order_id,
        stream_id_,
        trace_info,
        order_update,
        []([[maybe_unused]] auto &order) {});
  }
}

}  // namespace binance
}  // namespace roq
