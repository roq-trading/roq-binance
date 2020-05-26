/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/market_stream.h"

#include <fmt/format.h>

#include <cassert>
#include <utility>

#include "roq/builtins.h"
#include "roq/patterns.h"

#include "roq/core/clock.h"

#include "roq/core/charconv.h"

#include "roq/binance/options.h"

namespace roq {
namespace binance {

namespace {
constexpr std::string_view CONNECTION = "market_stream";

static auto create_connection_name(uint32_t market_stream_id) {
  return fmt::format(
      FMT_STRING("{}_{}"),
      CONNECTION,
      market_stream_id);
}

static auto create_counter(
    uint32_t market_stream_id,
    const std::string_view& function) {
  return core::metrics::Counter(
      FLAGS_name,
      create_connection_name(market_stream_id),
      function);
}

static auto create_profile(
    uint32_t market_stream_id,
    const std::string_view& function) {
  return core::metrics::Profile(
      FLAGS_name,
      create_connection_name(market_stream_id),
      function);
}

static auto create_latency(
    uint32_t market_stream_id,
    const std::string_view& function) {
  return core::metrics::Latency(
      FLAGS_name,
      create_connection_name(market_stream_id),
      function);
}
}  // namespace

MarketStream::MarketStream(
    Handler& handler,
    Random& random,
    core::event::Base& base,
    core::event::DNSBase& dns_base,
    core::ssl::Context& ssl_context,
    uint32_t market_stream_id,
    std::vector<std::string>&& symbols)
    : _handler(handler),
      _market_stream_id(market_stream_id),
      _symbols(std::move(symbols)),
      _random(random),
      _connection(
          *this,
          base,
          dns_base,
          ssl_context,
          core::URI(FLAGS_ws_uri),
          std::string_view(),  // query
          std::chrono::seconds { FLAGS_ws_ping_freq_secs },
          FLAGS_decode_buffer_size,
          FLAGS_encode_buffer_size,
          []() { return std::string(); }),
      _decode_buffer(FLAGS_decode_buffer_size),
      _counter {
        .disconnect = create_counter(market_stream_id, "disconnect"),
      },
      _profile {
        .parse = create_profile(market_stream_id, "parse"),
        .error = create_profile(market_stream_id, "error"),
        .result = create_profile(market_stream_id, "result"),
        .agg_trade = create_profile(market_stream_id, "agg_trade"),
        .trade = create_profile(market_stream_id, "trade"),
        .mini_ticker = create_profile(market_stream_id, "mini_ticker"),
        .book_ticker = create_profile(market_stream_id, "book_ticker"),
        .depth = create_profile(market_stream_id, "depth"),
        .depth_update = create_profile(market_stream_id, "depth_update"),
      },
      _latency {
        .ping = create_latency(market_stream_id, "ping"),
        .heartbeat = create_latency(market_stream_id, "heartbeat"),
      } {
}

bool MarketStream::ready() const {
  return _connection.ready();
}

void MarketStream::operator()(const StartEvent&) {
  _connection.start();
}

void MarketStream::operator()(const StopEvent&) {
  _connection.stop();
}

void MarketStream::operator()(const TimerEvent& event) {
  _connection.refresh(event.now);
}

template <>
void MarketStream::subscribe_agg_trade(
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
void MarketStream::subscribe_trade(
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
void MarketStream::subscribe_mini_ticker(
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
void MarketStream::subscribe_book_ticker(
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
void MarketStream::subscribe_depth(
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

void MarketStream::operator()(Metrics& metrics) {
  metrics
    // counter
    .write(_counter.disconnect)
    // profile
    .write(_profile.parse)
    .write(_profile.error)
    .write(_profile.result)
    .write(_profile.agg_trade)
    .write(_profile.trade)
    .write(_profile.mini_ticker)
    .write(_profile.book_ticker)
    .write(_profile.depth)
    .write(_profile.depth_update)
    // latency
    .write(_latency.ping)
    .write(_latency.heartbeat);
}

void MarketStream::operator()(const core::web::Socket::Connected&) {
  // _handler(*this);
}

void MarketStream::operator()(const core::web::Socket::Disconnected&) {
  // _handler(*this);
  ++_counter.disconnect;
}

void MarketStream::operator()(const core::web::Socket::Ready&) {
  LOG(INFO)(
      FMT_STRING("Ready (#{})"),
      _market_stream_id);
  if (FLAGS_ws_trade_details) {
    subscribe_trade(_symbols);
  } else {
    subscribe_agg_trade(_symbols);
  }
  subscribe_mini_ticker(_symbols);
  subscribe_book_ticker(_symbols);
  subscribe_depth(_symbols);
}

void MarketStream::operator()(const core::web::Socket::Close&) {
}

void MarketStream::operator()(const core::web::Socket::Latency& latency) {
  _latency.ping.update(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          latency.sample).count());
}

void MarketStream::operator()(const core::web::Socket::Text& text) {
  parse(text.payload);
}

void MarketStream::parse(const std::string_view& message) {
  _profile.parse(
      [&]() {
        try {
          core::json::Buffer buffer(_decode_buffer);
          json::MarketStreamParser::dispatch(
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

void MarketStream::operator()(
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

void MarketStream::operator()(
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

void MarketStream::operator()(const json::AggTrade& agg_trade) {
  _profile.agg_trade(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(agg_trade={})"),
            agg_trade);
        _handler(agg_trade);
      });
}

void MarketStream::operator()(const json::Trade& trade) {
  _profile.trade(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(trade={})"),
            trade);
        _handler(trade);
      });
}

void MarketStream::operator()(const json::MiniTicker& mini_ticker) {
  _profile.mini_ticker(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(mini_ticker={})"),
            mini_ticker);
        _handler(mini_ticker);
      });
}

void MarketStream::operator()(const json::BookTicker& book_ticker) {
  _profile.book_ticker(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(book_ticker={})"),
            book_ticker);
        _handler(book_ticker);
      });
}

void MarketStream::operator()(
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

void MarketStream::operator()(
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
