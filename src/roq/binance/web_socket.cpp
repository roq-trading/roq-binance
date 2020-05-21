/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/web_socket.h"

#include <fmt/format.h>

#include <cassert>

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
        .error = create_profile("error"),
        .result = create_profile("result"),
        .agg_trade = create_profile("agg_trade"),
        .trade = create_profile("trade"),
        .mini_ticker = create_profile("mini_ticker"),
        .book_ticker = create_profile("book_ticker"),
        .depth = create_profile("depth"),
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
}

template <>
void WebSocket::subscribe_agg_trade(
    const std::vector<std::string>& symbols) {
  assert(symbols.empty() == false);
  auto message = fmt::format(
      FMT_STRING(
        R"({{)"
        R"("method":"SUBSCRIBE",)"
        R"("params":["{}@aggTrade"],)"
        R"("id":{})"
        R"(}})"),
      fmt::join(
          symbols,
          R"(@aggTrade",")"),
      ++_request_id);
  _connection.send_text(message);
}

template <>
void WebSocket::subscribe_trade(
    const std::vector<std::string>& symbols) {
  assert(symbols.empty() == false);
  auto message = fmt::format(
      FMT_STRING(
        R"({{)"
        R"("method":"SUBSCRIBE",)"
        R"("params":["{}@trade"],)"
        R"("id":{})"
        R"(}})"),
      fmt::join(
          symbols,
          R"(@trade",")"),
      ++_request_id);
  _connection.send_text(message);
}

template <>
void WebSocket::subscribe_mini_ticker(
    const std::vector<std::string>& symbols) {
  assert(symbols.empty() == false);
  auto message = fmt::format(
      FMT_STRING(
        R"({{)"
        R"("method":"SUBSCRIBE",)"
        R"("params":["{}@miniTicker"],)"
        R"("id":{})"
        R"(}})"),
      fmt::join(
          symbols,
          R"(@miniTicker",")"),
      ++_request_id);
  _connection.send_text(message);
}

template <>
void WebSocket::subscribe_book_ticker(
    const std::vector<std::string>& symbols) {
  assert(symbols.empty() == false);
  auto message = fmt::format(
      FMT_STRING(
        R"({{)"
        R"("method":"SUBSCRIBE",)"
        R"("params":["{}@bookTicker"],)"
        R"("id":{})"
        R"(}})"),
      fmt::join(
          symbols,
          R"(@bookTicker",")"),
      ++_request_id);
  _connection.send_text(message);
}

template <>
void WebSocket::subscribe_depth(
    const std::vector<std::string>& symbols) {
  assert(symbols.empty() == false);
  auto stream = fmt::format(
      FMT_STRING(R"(@depth{}@{}ms)"),
      FLAGS_ws_depth_levels,
      FLAGS_ws_depth_freq_msecs);
  auto separator = fmt::format(
      FMT_STRING(R"({}",")"),
      stream);
  auto message = fmt::format(
      FMT_STRING(
        R"({{)"
        R"("method":"SUBSCRIBE",)"
        R"("params":["{}{}"],)"
        R"("id":{})"
        R"(}})"),
      fmt::join(
          symbols,
          separator),
      stream,
      ++_request_id);
  _connection.send_text(message);
}

void WebSocket::operator()(Metrics& metrics) {
  metrics
    // counter
    .write(_counter.disconnect)
    // profile
    .write(_profile.parse)
    .write(_profile.error)
    .write(_profile.result)
    .write(_profile.agg_trade)
    .write(_profile.trade)
    .write(_profile.book_ticker)
    .write(_profile.depth)
    .write(_profile.depth_update)
    // latency
    .write(_latency.ping)
    .write(_latency.heartbeat);
}

void WebSocket::operator()(const core::web::Socket::Connected&) {
  _handler(*this);
}

void WebSocket::operator()(const core::web::Socket::Disconnected&) {
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

void WebSocket::operator()(
    int32_t id,
    const json::Error& error) {
  _profile.error(
      [&]() {
        LOG(WARNING)(
            FMT_STRING(R"(id={}, error={})"),
            id,
            error);
      });
}

void WebSocket::operator()(
    int32_t id,
    const json::Result& result) {
  _profile.result(
      [&]() {
        LOG(INFO)(
            FMT_STRING(R"(id={}, result={})"),
            id,
            result);
      });
}

void WebSocket::operator()(const json::AggTrade& agg_trade) {
  _profile.agg_trade(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(agg_trade={})"),
            agg_trade);
        _handler(agg_trade);
      });
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

void WebSocket::operator()(const json::MiniTicker& mini_ticker) {
  _profile.mini_ticker(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(mini_ticker={})"),
            mini_ticker);
        _handler(mini_ticker);
      });
}

void WebSocket::operator()(const json::BookTicker& book_ticker) {
  _profile.book_ticker(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(book_ticker={})"),
            book_ticker);
        _handler(book_ticker);
      });
}

void WebSocket::operator()(
    const std::string_view& symbol,
    const json::Depth& depth) {
  _profile.depth(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(symbol="{}", depth={})"),
            symbol,
            depth);
        _handler(
            symbol,
            depth);
      });
}

void WebSocket::operator()(
    const std::string_view& symbol,
    const json::DepthUpdate& depth_update) {
  _profile.depth_update(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(symbol="{}", depth_update={})"),
            symbol,
            depth_update);
        _handler(
            symbol,
            depth_update);
      });
}

}  // namespace binance
}  // namespace roq
