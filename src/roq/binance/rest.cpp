/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/rest.h"

#include <fmt/chrono.h>

#include <utility>

#include "roq/binance/flags.h"

#include "roq/binance/json/account.h"
#include "roq/binance/json/depth.h"
#include "roq/binance/json/exchange_info.h"
#include "roq/binance/json/listen_key.h"

#include "roq/binance/json/utils.h"

using namespace roq::literals;

namespace roq {
namespace binance {

namespace {
static const auto CONNECTION = "rest"_sv;

static const auto ACCEPT_ALL = "*/*"_sv;
static const auto ACCEPT_JSON = "application/json"_sv;
static const auto CONTENT_TYPE_JSON = "application/json"_sv;

class create_metrics final {
 public:
  explicit create_metrics(const std::string_view &function) : function_(function) {}
  create_metrics(create_metrics &&) = default;
  create_metrics(const create_metrics &) = delete;
  template <typename T>
  operator T() {
    return T(Flags::name(), CONNECTION, function_);
  }

 private:
  std::string_view function_;
};
}  // namespace

Rest::Rest(
    Handler &handler,
    [[maybe_unused]] const Config &config,
    Random &random,
    core::io::Context &context)
    : handler_(handler), random_(random), api_key_(config.get_api_key()),
      connection_(
          *this,
          context,
          core::URI(Flags::rest_uri()),
          ROQ_PACKAGE_NAME,
          true,  // keep alive
          Flags::rest_request_queue_depth(),
          std::chrono::seconds{Flags::rest_request_timeout_secs()},
          std::chrono::seconds{Flags::rest_rate_limit_interval_secs()},
          Flags::rest_rate_limit_max_requests(),
          std::chrono::seconds{Flags::rest_ping_freq_secs()},
          Flags::decode_buffer_size(),
          Flags::encode_buffer_size(),
          Flags::rest_ping_path()),
      decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_metrics("disconnect"_sv),
      },
      profile_{
          .exchange_info = create_metrics("exchange_info"_sv),
          .account = create_metrics("account"_sv),
          .listen_key = create_metrics("listen_key"_sv),
          .depth = create_metrics("depth"_sv),
          .new_order = create_metrics("new_order"_sv),
          .cancel_order = create_metrics("cancel_order"_sv),
      },
      latency_{
          .ping = create_metrics("ping"_sv),
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
void Rest::get(std::function<void(const core::Promise<json::ExchangeInfo> &)> &&callback) {
  constexpr auto method = core::http::Method::GET;
  constexpr std::string_view path = "/api/v3/exchangeInfo"_sv;
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      ACCEPT_ALL,
      std::string_view(),  // content_type
      std::string_view(),  // headers
      std::string_view(),  // body
      [this, callback{std::move(callback)}](auto &response) {
        profile_.exchange_info([&]() {
          try {
            response.expect(core::http::Status::OK);
            core::json::Buffer buffer(decode_buffer_);
            auto exchange_info =
                core::json::Parser::create<json::ExchangeInfo>(response.body(), buffer);
            VLOG(1)(R"(exchange_info={})"_fmt, exchange_info);
            core::Promise<json::ExchangeInfo> promise(exchange_info);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")"_fmt, typeid(e).name(), e.what());
            core::Promise<json::ExchangeInfo> promise(std::current_exception());
            callback(promise);
          }
        });
      });
}

template <>
void Rest::get(std::function<void(const core::Promise<json::Account> &)> &&callback) {
  constexpr auto method = core::http::Method::GET;
  constexpr std::string_view path = "/api/v3/account"_sv;
  auto now = core::get_realtime_clock();
  auto timestamp = roq::format(
      R"(timestamp={})"_fmt, std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
  auto signature = random_.create_signature(timestamp);
  auto query = roq::format(R"(?{}&signature={})"_fmt, timestamp, signature);
  auto headers = roq::format("X-MBX-APIKEY: {}\r\n"_fmt, api_key_);
  connection_.request(
      method,
      path,
      query,
      ACCEPT_ALL,
      std::string_view(),  // content_type
      headers,
      std::string_view(),  // body
      [this, callback{std::move(callback)}](auto &response) {
        profile_.account([&]() {
          try {
            response.expect(core::http::Status::OK);
            core::json::Buffer buffer(decode_buffer_);
            auto account = core::json::Parser::create<json::Account>(response.body(), buffer);
            VLOG(1)(R"(account={})"_fmt, account);
            core::Promise<json::Account> promise(account);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")"_fmt, typeid(e).name(), e.what());
            core::Promise<json::Account> promise(std::current_exception());
            callback(promise);
          }
        });
      });
}

template <>
void Rest::get(std::function<void(const core::Promise<json::ListenKey> &)> &&callback) {
  constexpr auto method = core::http::Method::POST;
  constexpr std::string_view path = "/api/v3/userDataStream"_sv;
  auto headers = roq::format("X-MBX-APIKEY: {}\r\n"_fmt, api_key_);
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      ACCEPT_ALL,
      std::string_view(),  // content_type
      headers,
      std::string_view(),  // body
      [this, callback{std::move(callback)}](auto &response) {
        profile_.listen_key([&]() {
          try {
            response.expect(core::http::Status::OK);
            auto listen_key = core::json::Parser::create<json::ListenKey>(response.body());
            VLOG(1)(R"(listen_key={})"_fmt, listen_key);
            core::Promise<json::ListenKey> promise(listen_key);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")"_fmt, typeid(e).name(), e.what());
            core::Promise<json::ListenKey> promise(std::current_exception());
            callback(promise);
          }
        });
      });
}

template <>
void Rest::get(std::function<void(const core::Promise<json::Depth> &)> &&callback) {
  assert(false);  // XXX not ready
  constexpr auto method = core::http::Method::GET;
  constexpr std::string_view path = "/api/v3/depth?symbol=BTCUSDT"_sv;  // XXX DEBUG
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      ACCEPT_ALL,
      std::string_view(),  // content_type
      std::string_view(),  // headers
      std::string_view(),  // body
      [this, callback{std::move(callback)}](auto &response) {
        profile_.depth([&]() {
          try {
            response.expect(core::http::Status::OK);
            core::json::Buffer buffer(decode_buffer_);
            auto depth = core::json::Parser::create<json::Depth>(response.body(), buffer);
            VLOG(1)(R"(depth={})"_fmt, depth);
            core::Promise<json::Depth> promise(depth);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")"_fmt, typeid(e).name(), e.what());
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
  constexpr std::string_view path = "/api/v3/order"_sv;
  auto timestamp = core::get_realtime_clock();
  // XXX use encode buffer
  auto message = roq::format(
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
      R"(}})"_fmt,
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
      Flags::rest_order_recv_window_msecs(),
      timestamp.count());
  DLOG(INFO)(R"(body="{}")"_fmt, message);
  auto headers = roq::format("X-MBX-APIKEY: {}\r\n"_fmt, api_key_);
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      ACCEPT_JSON,
      CONTENT_TYPE_JSON,
      headers,
      std::string_view(),  // body
      [this, callback{std::move(callback)}](auto &response) {
        profile_.new_order([&]() {
          try {
            response.expect(core::http::Status::OK);
            core::json::Buffer buffer(decode_buffer_);
            auto new_order = core::json::Parser::create<json::NewOrder>(response.body(), buffer);
            VLOG(1)(R"(new_order={})"_fmt, new_order);
            core::Promise<json::NewOrder> promise(new_order);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")"_fmt, typeid(e).name(), e.what());
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
  constexpr std::string_view path = "/api/v3/order"_sv;
  auto timestamp = core::get_realtime_clock();
  // XXX use encode buffer
  auto message = roq::format(
      R"({{)"
      R"("symbol":"{}",)"
      R"("origClientOrderId":"{}")"
      R"("newClientOrderId":"{}")"
      R"("recvWindow":{},)"
      R"("timestamp":{})"
      R"(}})"_fmt,
      order.symbol,
      order.external_order_id,
      request_id,
      Flags::rest_order_recv_window_msecs(),
      timestamp.count());
  DLOG(INFO)(R"(body="{}")"_fmt, message);
  auto headers = roq::format("X-MBX-APIKEY: {}\r\n"_fmt, api_key_);
  connection_.request(
      method,
      path,
      std::string_view(),  // query
      ACCEPT_JSON,
      CONTENT_TYPE_JSON,
      headers,
      std::string_view(),  // body
      [this, callback{std::move(callback)}](auto &response) {
        profile_.cancel_order([&]() {
          try {
            response.expect(core::http::Status::OK);
            auto cancel_order = core::json::Parser::create<json::CancelOrder>(response.body());
            VLOG(1)(R"(cancel_order={})"_fmt, cancel_order);
            core::Promise<json::CancelOrder> promise(cancel_order);
            callback(promise);
          } catch (NetworkError &e) {
            LOG(WARNING)
            (R"(Exception type={}, what="{}")"_fmt, typeid(e).name(), e.what());
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
  server::TraceInfo trace_info;
  ExternalLatency external_latency{
      .name = CONNECTION,
      .latency = latency.sample,
  };
  handler_(external_latency, trace_info);
  latency_.ping.update(latency.sample);
}

}  // namespace binance
}  // namespace roq
