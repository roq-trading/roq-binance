/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/web_socket.h"

#include <fmt/format.h>

#include "roq/builtins.h"
#include "roq/patterns.h"

#include "roq/core/clock.h"

#include "roq/core/charconv.h"

#include "roq/binance/options.h"

namespace roq {
namespace binance {

namespace {
constexpr std::string_view CONNECTION = "ws";

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

WebSocket::WebSocket(
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
          core::URI(FLAGS_ws_uri),
          std::chrono::seconds { FLAGS_ws_ping_freq_secs },
          FLAGS_decode_buffer_size,
          FLAGS_encode_buffer_size,
          []() { return std::string(); }),
      _decode_buffer(FLAGS_decode_buffer_size),
      _counter {
        .disconnect = create_counter("disconnect"),
      },
      _profile {
        .parse = create_profile("parse"),
        .trade = create_profile("trade"),
        .ticker = create_profile("ticker"),
        .depth_update = create_profile("depth_update"),
      },
      _latency {
        .ping = create_latency("ping"),
        .heartbeat = create_latency("heartbeat"),
      } {
  (void) config;  // avoid warning
}

bool WebSocket::ready() const {
  return _connection.ready();
}

void WebSocket::operator()(const StartEvent&) {
  _connection.start();
}

void WebSocket::operator()(const StopEvent&) {
  _connection.stop();
}

void WebSocket::operator()(const TimerEvent& event) {
  _connection.refresh(event.now);
  /*
  if (_connection.ready()) {
    if (FLAGS_cancel_all_after_secs &&
        _next_cancel_all_after <= event.now) {
      _next_cancel_all_after = event.now +
        std::chrono::seconds { FLAGS_cancel_all_after_secs / 4 };
      send_cancel_all_after();
    }
  }
  */
}

void WebSocket::subscribe() {
  auto message = fmt::format(
      FMT_STRING(
        R"({{)"
        R"("method":"SUBSCRIBE",)"
        R"("params":["btcusdt@trade","btcusdt@bookTicker","btcusdt@depth@100ms"],)"
        R"("id":{})"
        R"(}})"),
      1);
  _connection.send_text(message);
}

void WebSocket::operator()(Metrics& metrics) {
  metrics
    // counter
    .write(_counter.disconnect)
    // profile
    .write(_profile.parse)
    .write(_profile.trade)
    .write(_profile.ticker)
    .write(_profile.depth_update)
    // latency
    .write(_latency.ping)
    .write(_latency.heartbeat);
}

void WebSocket::operator()(const core::web::Socket::Connected&) {
  _handler(*this);
}

void WebSocket::operator()(const core::web::Socket::Disconnected&) {
  _next_cancel_all_after = {};
  _handler(*this);
  ++_counter.disconnect;
}

void WebSocket::operator()(const core::web::Socket::Ready&) {
  _handler(*this);
}

void WebSocket::operator()(const core::web::Socket::Close&) {
}

void WebSocket::operator()(const core::web::Socket::Latency& latency) {
  _latency.ping.update(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          latency.sample).count());
}

void WebSocket::operator()(const core::web::Socket::Text& text) {
  parse(text.payload);
}

void WebSocket::parse(const std::string_view& message) {
  _profile.parse(
      [&]() {
        try {
          core::json::Buffer buffer(_decode_buffer);
          json::Parser::dispatch(
              *this,
              message,
              buffer);
        } catch (std::exception& e) {
          LOG(WARNING)(
              FMT_STRING(R"(message="{}")"),
              message);
          LOG(FATAL)(
              FMT_STRING(R"(ERROR what="{}")"),
              e.what());
        }
      });
}

void WebSocket::operator()(int32_t, const json::Error&) {
}

void WebSocket::operator()(int32_t, const json::Result&) {
}

void WebSocket::operator()(const json::Trade& trade) {
  _profile.trade(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(trade={})"),
            trade);
        _handler(trade);
      });
}

void WebSocket::operator()(const json::BookTicker& ticker) {
  _profile.ticker(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(ticker={})"),
            ticker);
        _handler(ticker);
      });
}

void WebSocket::operator()(const json::DepthUpdate& depth_update) {
  _profile.depth_update(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(depth_update={})"),
            depth_update);
        _handler(depth_update);
      });
}

void WebSocket::send_cancel_all_after() {
  /*
  auto message = fmt::format(
      FMT_STRING(
        "{{"
        "\"op\":\"cancelAllAfter\","
        "\"args\":{}"
        "}}"),
      FLAGS_cancel_all_after_secs * 1000);  // milliseconds
  _connection.send_text(message);
  */
}

}  // namespace binance
}  // namespace roq
