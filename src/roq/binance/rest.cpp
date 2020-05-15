/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/rest.h"

#include <fmt/format.h>
#include <fmt/chrono.h>

#include "roq/binance/options.h"

#include "roq/binance/json/exchange_info.h"

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
      std::string_view(),  // headers
      std::string_view(),  // body
      [this, callback](auto& response) {
    _profile.exchange_info(
        [&]() {
      try {
        response.expect(core::http::Status::OK);
        core::json::Buffer buffer(_decode_buffer);
        auto products = core::json::Parser::create<json::ExchangeInfo>(
            response.body(),
            buffer);
        VLOG(1)(
            FMT_STRING(R"(exchange_info={})"),
            products);
        core::Promise<json::ExchangeInfo> promise(products);
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
