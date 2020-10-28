/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/rest.h"

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "roq/binance/options.h"

#include "roq/binance/json/account.h"
#include "roq/binance/json/depth.h"
#include "roq/binance/json/exchange_info.h"
#include "roq/binance/json/listen_key.h"

#include "roq/binance/json/utils.h"

namespace roq {
namespace binance {

namespace {
constexpr std::string_view CONNECTION = "rest";

static auto create_counter(const std::string_view &function) {
  return core::metrics::Counter(FLAGS_name, CONNECTION, function);
}

static auto create_profile(const std::string_view &function) {
  return core::metrics::Profile(FLAGS_name, CONNECTION, function);
}

static auto create_latency(const std::string_view &function) {
  return core::metrics::Latency(FLAGS_name, CONNECTION, function);
}
}  // namespace

Rest::Rest(
    Handler &handler,
    [[maybe_unused]] const Config &config,
    Random &random,
    core::event::Base &base,
    core::event::DNSBase &dns_base,
    core::ssl::Context &ssl_context)
    : handler_(handler), random_(random), api_key_(config.get_api_key()),
      connection_(
          *this,
          base,
          dns_base,
          ssl_context,
          core::URI(FLAGS_rest_uri),
          ROQ_PACKAGE_NAME,
          true,  // keep alive
          FLAGS_rest_request_queue_depth,
          std::chrono::seconds{FLAGS_rest_request_timeout_secs},
          std::chrono::seconds{FLAGS_rest_rate_limit_interval_secs},
          FLAGS_rest_rate_limit_max_requests,
          std::chrono::seconds{FLAGS_rest_ping_freq_secs},
          FLAGS_decode_buffer_size,
          FLAGS_encode_buffer_size,
          FLAGS_rest_ping_path),
      decode_buffer_(FLAGS_decode_buffer_size),
      counter_{
          .disconnect = create_counter("disconnect"),
      },
      profile_{
          .exchange_info = create_profile("exchange_info"),
          .account = create_profile("account"),
          .listen_key = create_profile("listen_key"),
          .depth = create_profile("depth"),
          .new_order = create_profile("new_order"),
          .cancel_order = create_profile("cancel_order"),
      },
      latency_{
          .ping = create_latency("ping"),
      } {
}

bool Rest::ready() const {
  return connection_.ready();
}

void Rest::operator()(const Event<Start> &) {
  connection_.start();
}

void Rest::operator()(const Event<Stop> &) {
  connection_.stop();
}

void Rest::operator()(const Event<Timer> &event) {
  connection_.refresh(event.value.now);
}

void Rest::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.exchange_info, metrics::PROFILE)
      .write(profile_.account, metrics::PROFILE)
      .write(profile_.listen_key, metrics::PROFILE)
      .write(profile_.depth, metrics::PROFILE)
      .write(profile_.new_order, metrics::PROFILE)
      .write(profile_.cancel_order, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY);
}

template <>
void Rest::get(
    std::function<void(const core::Promise<json::ExchangeInfo> &)> &&callback) {
  constexpr auto method = core::http::Method::GET;
  constexpr std::string_view path = "/api/v3/exchangeInfo";
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      std::string_view(),  // headers
      std::string_view(),  // body
      [this, callback](auto &response) {
        profile_.exchange_info([&]() {
          try {
            response.expect(core::http::Status::OK);
            core::json::Buffer buffer(decode_buffer_);
            auto exchange_info = core::json::Parser::create<json::ExchangeInfo>(
                response.body(), buffer);
            VLOG(1)(R"(exchange_info={})", exchange_info);
            core::Promise<json::ExchangeInfo> promise(exchange_info);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")", typeid(e).name(), e.what());
            core::Promise<json::ExchangeInfo> promise(std::current_exception());
            callback(promise);
          }
        });
      });
}

template <>
void Rest::get(
    std::function<void(const core::Promise<json::Account> &)> &&callback) {
  constexpr auto method = core::http::Method::GET;
  constexpr std::string_view path = "/api/v3/account";
  auto now = core::get_realtime_clock();
  auto timestamp = fmt::format(
      R"(timestamp={})",
      std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
  auto signature = random_.create_signature(timestamp);
  auto query = fmt::format(R"(?{}&signature={})", timestamp, signature);
  auto headers = fmt::format("X-MBX-APIKEY: {}\r\n", api_key_);
  connection_.request(
      method,
      path,
      query,
      headers,
      std::string_view(),  // body
      [this, callback](auto &response) {
        profile_.account([&]() {
          try {
            response.expect(core::http::Status::OK);
            core::json::Buffer buffer(decode_buffer_);
            auto account = core::json::Parser::create<json::Account>(
                response.body(), buffer);
            VLOG(1)(R"(account={})", account);
            core::Promise<json::Account> promise(account);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")", typeid(e).name(), e.what());
            core::Promise<json::Account> promise(std::current_exception());
            callback(promise);
          }
        });
      });
}

template <>
void Rest::get(
    std::function<void(const core::Promise<json::ListenKey> &)> &&callback) {
  constexpr auto method = core::http::Method::POST;
  constexpr std::string_view path = "/api/v3/userDataStream";
  auto headers = fmt::format("X-MBX-APIKEY: {}\r\n", api_key_);
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      headers,
      std::string_view(),  // body
      [this, callback](auto &response) {
        profile_.listen_key([&]() {
          try {
            response.expect(core::http::Status::OK);
            auto listen_key =
                core::json::Parser::create<json::ListenKey>(response.body());
            VLOG(1)(R"(listen_key={})", listen_key);
            core::Promise<json::ListenKey> promise(listen_key);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")", typeid(e).name(), e.what());
            core::Promise<json::ListenKey> promise(std::current_exception());
            callback(promise);
          }
        });
      });
}

template <>
void Rest::get(
    std::function<void(const core::Promise<json::Depth> &)> &&callback) {
  assert(false);  // XXX not ready
  constexpr auto method = core::http::Method::GET;
  constexpr std::string_view path =
      "/api/v3/depth?symbol=BTCUSDT";  // XXX DEBUG
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      std::string_view(),  // headers
      std::string_view(),  // body
      [this, callback](auto &response) {
        profile_.depth([&]() {
          try {
            response.expect(core::http::Status::OK);
            core::json::Buffer buffer(decode_buffer_);
            auto depth = core::json::Parser::create<json::Depth>(
                response.body(), buffer);
            VLOG(1)(R"(depth={})", depth);
            core::Promise<json::Depth> promise(depth);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")", typeid(e).name(), e.what());
            core::Promise<json::Depth> promise(std::current_exception());
            callback(promise);
          }
        });
      });
}

void Rest::create_order(
    const CreateOrder &create_order,
    const std::string_view &cl_ord_id,
    std::function<void(const core::Promise<json::NewOrder> &)> &&callback) {
  constexpr auto method = core::http::Method::POST;
  constexpr std::string_view path = "/api/v3/order";
  auto timestamp = core::get_realtime_clock();
  // XXX use encode buffer
  auto message = fmt::format(
      R"({{)"
      R"("symbol":"{}",)"
      R"("side":"{}",)"
      R"("type":"{}",)"
      R"("timeInForce":"{}",)"
      R"("quantity":{},)"
      R"("quoteOrderQty":{},)"  // XXX ???
      R"("price":{},)"
      R"("newClientOrderId":"{}")"
      R"("stopPrice":{},)"   // XXX ???
      R"("icebergQty":{},)"  // XXX ???
      R"("recvWindow":{},)"
      R"("timestamp":{})"
      R"(}})",
      create_order.symbol,
      json::map(create_order.side).as_raw_text(),
      json::map(create_order.order_type).as_raw_text(),
      json::map(create_order.time_in_force).as_raw_text(),
      create_order.quantity,
      double{0.0},
      create_order.price,
      cl_ord_id,
      double{0.0},
      double{0.0},
      FLAGS_rest_recv_window_secs * 1000,
      timestamp.count());
  DLOG(INFO)(R"(body="{}")", message);
  auto headers = fmt::format("X-MBX-APIKEY: {}\r\n", api_key_);
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      headers,
      std::string_view(),  // body
      [this, callback](auto &response) {
        profile_.new_order([&]() {
          try {
            response.expect(core::http::Status::OK);
            core::json::Buffer buffer(decode_buffer_);
            auto new_order = core::json::Parser::create<json::NewOrder>(
                response.body(), buffer);
            VLOG(1)(R"(new_order={})", new_order);
            core::Promise<json::NewOrder> promise(new_order);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")", typeid(e).name(), e.what());
            core::Promise<json::NewOrder> promise(std::current_exception());
            callback(promise);
          }
        });
      });
}

void Rest::cancel_order(
    [[maybe_unused]] const CancelOrder &cancel_order,
    const std::string_view &request_id,
    const server::OMS_Order &order,
    std::function<void(const core::Promise<json::CancelOrder> &)> &&callback) {
  constexpr auto method = core::http::Method::DELETE;
  constexpr std::string_view path = "/api/v3/order";
  auto timestamp = core::get_realtime_clock();
  // XXX use encode buffer
  auto message = fmt::format(
      R"({{)"
      R"("symbol":"{}",)"
      R"("origClientOrderId":"{}")"
      R"("newClientOrderId":"{}")"
      R"("recvWindow":{},)"
      R"("timestamp":{})"
      R"(}})",
      order.symbol,
      order.external_order_id,
      request_id,
      FLAGS_rest_recv_window_secs * 1000,
      timestamp.count());
  DLOG(INFO)(R"(body="{}")", message);
  auto headers = fmt::format("X-MBX-APIKEY: {}\r\n", api_key_);
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      headers,
      std::string_view(),  // body
      [this, callback](auto &response) {
        profile_.cancel_order([&]() {
          try {
            response.expect(core::http::Status::OK);
            auto cancel_order =
                core::json::Parser::create<json::CancelOrder>(response.body());
            VLOG(1)(R"(cancel_order={})", cancel_order);
            core::Promise<json::CancelOrder> promise(cancel_order);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")", typeid(e).name(), e.what());
            core::Promise<json::CancelOrder> promise(std::current_exception());
            callback(promise);
          }
        });
      });
}

void Rest::operator()(const core::web::Client::Connected &) {
  handler_(*this);
}

void Rest::operator()(const core::web::Client::Disconnected &) {
  handler_(*this);
  ++counter_.disconnect;
}

void Rest::operator()(const core::web::Client::Latency &latency) {
  latency_.ping.update(
      std::chrono::duration_cast<std::chrono::nanoseconds>(latency.sample)
          .count());
}

}  // namespace binance
}  // namespace roq
