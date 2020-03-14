/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/web_socket.h"

#include <fmt/format.h>

#include "roq/builtins.h"
#include "roq/patterns.h"

#include "roq/core/clock.h"

#include "roq/core/charconv.h"

#include "roq/binance/gateway.h"
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
    Gateway& gateway,
    const Config& config,
    Random& random,
    core::event::Base& base,
    core::event::DNSBase& dns_base,
    core::ssl::Context& ssl_context)
    : _gateway(gateway),
      _random(random),
      _connection(
          *this,
          base,
          dns_base,
          ssl_context,
          core::URI(FLAGS_ws_uri),
          std::chrono::seconds { FLAGS_ping_freq_secs },
          FLAGS_decode_buffer_size,
          FLAGS_encode_buffer_size,
          []() { return std::string(); }),
      _decode_buffer(FLAGS_decode_buffer_size),
      _counter {
        .disconnect = create_counter("disconnect"),
      },
      _profile {
        .parse = create_profile("parse"),
        .cancel_all_after = create_profile("cancel_all_after"),
        .error = create_profile("error"),
        .execution = create_profile("execution"),
        .funding = create_profile("funding"),
        .handshake = create_profile("handshake"),
        .instrument = create_profile("instrument"),
        .liquidation = create_profile("liquidation"),
        .margin = create_profile("margin"),
        .order = create_profile("order"),
        .order_book_l2 = create_profile("order_book_l2"),
        .position = create_profile("position"),
        .quote = create_profile("quote"),
        .settlement = create_profile("settlement"),
        .subscribe = create_profile("subscribe"),
        .trade = create_profile("trade"),
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
  if (_connection.ready()) {
    if (FLAGS_cancel_all_after_secs &&
        _next_cancel_all_after <= event.now) {
      _next_cancel_all_after = event.now +
        std::chrono::seconds { FLAGS_cancel_all_after_secs / 4 };
      send_cancel_all_after();
    }
  }
}

void WebSocket::subscribe(const std::string_view& topic) {
  auto message = fmt::format(
      FMT_STRING(
        "{{"
        "\"op\":\"subscribe\","
        "\"args\":"
        "\"{}\""
        "}}"),
      topic);
  _connection.send_text(message);
}

void WebSocket::subscribe(
    const std::string_view& topic,
    const std::vector<std::string>& filter) {
  if (filter.empty()) {
    subscribe(topic);
  } else {
    auto message = fmt::format(
        FMT_STRING(
          "{{"
          "\"op\":\"subscribe\","
          "\"args\":"
          "\"{}:{}\""
          "}}"),
        topic,
        fmt::join(filter, ","));
    _connection.send_text(message);
  }
}

void WebSocket::operator()(Metrics& metrics) {
  metrics
    // counter
    .write(_counter.disconnect)
    // profile
    .write(_profile.parse)
    .write(_profile.cancel_all_after)
    .write(_profile.error)
    .write(_profile.execution)
    .write(_profile.funding)
    .write(_profile.handshake)
    .write(_profile.instrument)
    .write(_profile.liquidation)
    .write(_profile.margin)
    .write(_profile.order)
    .write(_profile.order_book_l2)
    .write(_profile.position)
    .write(_profile.quote)
    .write(_profile.settlement)
    .write(_profile.subscribe)
    .write(_profile.trade)
    // latency
    .write(_latency.ping)
    .write(_latency.heartbeat);
}

void WebSocket::operator()(const core::web::Socket::Connected&) {
  _gateway(*this);
}

void WebSocket::operator()(const core::web::Socket::Disconnected&) {
  _next_cancel_all_after = {};
  _gateway(*this);
  ++_counter.disconnect;
}

void WebSocket::operator()(const core::web::Socket::Ready&) {
  _gateway(*this);
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
  VLOG(4)(
      FMT_STRING("message={}"),
      message);
  _profile.parse(
      [&]() {
        try {
          parse_helper(message);
        } catch (std::exception& e) {
          LOG(FATAL)(
              FMT_STRING("ERROR what=\"{}\""),
              e.what());
        }
      });
}

void WebSocket::parse_helper(const std::string_view& message) {
  core::json::Buffer buffer(_decode_buffer);
  json::Parser::dispatch(
      *this,
      message,
      buffer);
}

void WebSocket::send_cancel_all_after() {
  auto message = fmt::format(
      FMT_STRING(
        "{{"
        "\"op\":\"cancelAllAfter\","
        "\"args\":{}"
        "}}"),
      FLAGS_cancel_all_after_secs * 1000);  // milliseconds
  _connection.send_text(message);
}

}  // namespace binance
}  // namespace roq
