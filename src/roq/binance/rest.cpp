/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/rest.h"

#include <fmt/format.h>
#include <fmt/chrono.h>

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

static auto create_counter(
    const std::string_view& function) {
  return core::metrics::Counter(
      FLAGS_name,
      CONNECTION,
      function);
}

static auto create_profile(
    const std::string_view& function) {
  return core::metrics::Profile(
      FLAGS_name,
      CONNECTION,
      function);
}

static auto create_latency(
    const std::string_view& function) {
  return core::metrics::Latency(
      FLAGS_name,
      CONNECTION,
      function);
}
}  // namespace

Rest::Rest(
    Handler& handler,
    const Config& config,
    Random& random,
    core::event::Base& base,
    core::event::DNSBase& dns_base,
    core::ssl::Context& ssl_context)
    : _handler(handler),
      _random(random),
      _api_key(config.get_api_key()),
      _connection(
          *this,
          base,
          dns_base,
          ssl_context,
          core::URI(FLAGS_rest_uri),
          PACKAGE_NAME,
          true,  // keep alive
          std::chrono::seconds { FLAGS_rest_rate_limit_interval_secs },
          FLAGS_rest_rate_limit_max_requests,
          std::chrono::seconds { FLAGS_rest_ping_freq_secs },
          FLAGS_decode_buffer_size,
          FLAGS_encode_buffer_size,
          FLAGS_rest_ping_path),
      _decode_buffer(FLAGS_decode_buffer_size),
      _counter {
        .disconnect = create_counter("disconnect"),
      },
      _profile {
        .exchange_info = create_profile("exchange_info"),
        .account = create_profile("account"),
        .listen_key = create_profile("listen_key"),
        .depth = create_profile("depth"),
      },
      _latency {
        .ping = create_latency("ping"),
      } {
  (void) config;  // avoid warning
}

bool Rest::ready() const {
  return _connection.ready();
}

void Rest::operator()(const StartEvent&) {
  _connection.start();
}

void Rest::operator()(const StopEvent&) {
  _connection.stop();
}

void Rest::operator()(const TimerEvent& event) {
  _connection.refresh(event.now);
}

void Rest::operator()(Metrics& metrics) {
  metrics
    // counter
    .write(_counter.disconnect)
    // profile
    .write(_profile.exchange_info)
    .write(_profile.account)
    .write(_profile.listen_key)
    .write(_profile.depth)
    // latency
    .write(_latency.ping);
}

template <>
void Rest::get(
    std::function<void(const core::Promise<json::ExchangeInfo>&)>&& callback) {
  constexpr auto method = core::http::Method::GET;
  constexpr std::string_view path = "/api/v3/exchangeInfo";
  _connection.request(
      method,
      path,
      std::string_view(),  // query
      std::string_view(),  // headers
      std::string_view(),  // body
      [this, callback](auto& response) {
    _profile.exchange_info(
        [&]() {
      try {
        response.expect(core::http::Status::OK);
        core::json::Buffer buffer(_decode_buffer);
        auto exchange_info = core::json::Parser::create<json::ExchangeInfo>(
            response.body(),
            buffer);
        VLOG(1)(
            FMT_STRING(R"(exchange_info={})"),
            exchange_info);
        core::Promise<json::ExchangeInfo> promise(exchange_info);
        callback(promise);
      } catch (NetworkError& e) {
        LOG(WARNING)(
            FMT_STRING(R"(Exception type={}, what="{}")"),
            typeid(e).name(),
            e.what());
        core::Promise<json::ExchangeInfo> promise(std::current_exception());
        callback(promise);
      }
    });
  });
}

template <>
void Rest::get(
    std::function<void(const core::Promise<json::Account>&)>&& callback) {
  constexpr auto method = core::http::Method::GET;
  constexpr std::string_view path = "/api/v3/account";
  auto now = core::get_realtime_clock();
  auto timestamp = fmt::format(
      FMT_STRING(R"(timestamp={})"),
      std::chrono::duration_cast<
        std::chrono::milliseconds>(now).count());
  auto signature = _random.create_signature(timestamp);
  auto query = fmt::format(
      FMT_STRING(R"(?{}&signature={})"),
      timestamp,
      signature);
  auto headers = fmt::format(
      FMT_STRING("X-MBX-APIKEY: {}\r\n"),
      _api_key);
  _connection.request(
      method,
      path,
      query,
      headers,
      std::string_view(),  // body
      [this, callback](auto& response) {
    _profile.account(
        [&]() {
      try {
        response.expect(core::http::Status::OK);
        core::json::Buffer buffer(_decode_buffer);
        auto account = core::json::Parser::create<json::Account>(
            response.body(),
            buffer);
        VLOG(1)(
            FMT_STRING(R"(account={})"),
            account);
        core::Promise<json::Account> promise(account);
        callback(promise);
      } catch (NetworkError& e) {
        LOG(WARNING)(
            FMT_STRING(R"(Exception type={}, what="{}")"),
            typeid(e).name(),
            e.what());
        core::Promise<json::Account> promise(std::current_exception());
        callback(promise);
      }
    });
  });
}

template <>
void Rest::get(
    std::function<void(const core::Promise<json::ListenKey>&)>&& callback) {
  constexpr auto method = core::http::Method::POST;
  constexpr std::string_view path = "/api/v3/userDataStream";
  auto headers = fmt::format(
      FMT_STRING("X-MBX-APIKEY: {}\r\n"),
      _api_key);
  _connection.request(
      method,
      path,
      std::string_view(),  // query
      headers,
      std::string_view(),  // body
      [this, callback](auto& response) {
    _profile.listen_key(
        [&]() {
      try {
        response.expect(core::http::Status::OK);
        auto listen_key = core::json::Parser::create<json::ListenKey>(
            response.body());
        VLOG(1)(
            FMT_STRING(R"(listen_key={})"),
            listen_key);
        core::Promise<json::ListenKey> promise(listen_key);
        callback(promise);
      } catch (NetworkError& e) {
        LOG(WARNING)(
            FMT_STRING(R"(Exception type={}, what="{}")"),
            typeid(e).name(),
            e.what());
        core::Promise<json::ListenKey> promise(std::current_exception());
        callback(promise);
      }
    });
  });
}

template <>
void Rest::get(
    std::function<void(const core::Promise<json::Depth>&)>&& callback) {
  constexpr auto method = core::http::Method::GET;
  constexpr std::string_view path = "/api/v3/depth?symbol=BTCUSDT";  // XXX DEBUG
  _connection.request(
      method,
      path,
      std::string_view(),  // query
      std::string_view(),  // headers
      std::string_view(),  // body
      [this, callback](auto& response) {
    _profile.depth(
        [&]() {
      try {
        response.expect(core::http::Status::OK);
        core::json::Buffer buffer(_decode_buffer);
        auto products = core::json::Parser::create<json::Depth>(
            response.body(),
            buffer);
        VLOG(1)(
            FMT_STRING(R"(depth={})"),
            products);
        core::Promise<json::Depth> promise(products);
        callback(promise);
      } catch (NetworkError& e) {
        LOG(WARNING)(
            FMT_STRING(R"(Exception type={}, what="{}")"),
            typeid(e).name(),
            e.what());
        core::Promise<json::Depth> promise(std::current_exception());
        callback(promise);
      }
    });
  });
}

void Rest::operator()(const core::web::Client::Connected&) {
  _handler(*this);
}

void Rest::operator()(const core::web::Client::Disconnected&) {
  _handler(*this);
  ++_counter.disconnect;
}

void Rest::operator()(const core::web::Client::Latency& latency) {
  _latency.ping.update(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          latency.sample).count());
}

}  // namespace binance
}  // namespace roq
