/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/market_stream.h"

#include <fmt/format.h>

#include <cassert>
#include <utility>

#include "roq/core/patterns.h"

#include "roq/core/clock.h"

#include "roq/core/charconv.h"

#include "roq/binance/flags.h"

using namespace std::literals;  // NOLINT

namespace roq {
namespace binance {

namespace {
constexpr std::string_view CONNECTION = "market_stream"sv;

static auto create_connection_name(uint32_t market_stream_id) {
  return fmt::format("{}_{}"sv, CONNECTION, market_stream_id);
}

static auto create_counter(const std::string_view &name, const std::string_view &function) {
  return core::metrics::Counter(Flags::name(), name, function);
}

static auto create_profile(const std::string_view &name, const std::string_view &function) {
  return core::metrics::Profile(Flags::name(), name, function);
}

static auto create_latency(const std::string_view &name, const std::string_view &function) {
  return core::metrics::Latency(Flags::name(), name, function);
}
}  // namespace

MarketStream::MarketStream(
    Handler &handler,
    Random &random,
    core::event::Base &base,
    core::event::DNSBase &dns_base,
    core::ssl::Context &ssl_context,
    uint32_t market_stream_id,
    std::vector<std::string> &&symbols)
    : handler_(handler), market_stream_id_(market_stream_id), symbols_(std::move(symbols)),
      name_(create_connection_name(market_stream_id)), random_(random),
      connection_(
          *this,
          base,
          dns_base,
          ssl_context,
          core::URI(Flags::ws_uri()),
          std::string_view(),  // query
          std::chrono::seconds{Flags::ws_ping_freq_secs()},
          Flags::decode_buffer_size(),
          Flags::encode_buffer_size(),
          []() { return std::string(); }),
      decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_counter(name_, "disconnect"sv),
      },
      profile_{
          .parse = create_profile(name_, "parse"sv),
          .error = create_profile(name_, "error"sv),
          .result = create_profile(name_, "result"sv),
          .agg_trade = create_profile(name_, "agg_trade"sv),
          .trade = create_profile(name_, "trade"sv),
          .mini_ticker = create_profile(name_, "mini_ticker"sv),
          .book_ticker = create_profile(name_, "book_ticker"sv),
          .depth = create_profile(name_, "depth"sv),
          .depth_update = create_profile(name_, "depth_update"sv),
      },
      latency_{
          .ping = create_latency(name_, "ping"sv),
          .heartbeat = create_latency(name_, "heartbeat"sv),
      } {
}

bool MarketStream::ready() const {
  return connection_.ready();
}

void MarketStream::operator()(const Event<Start> &) {
  connection_.start();
}

void MarketStream::operator()(const Event<Stop> &) {
  connection_.stop();
}

void MarketStream::operator()(const Event<Timer> &event) {
  connection_.refresh(event.value.now);
}

size_t MarketStream::capacity() const {
  assert(Flags::ws_max_subscriptions_per_stream() > 0);
  size_t max_length = Flags::ws_max_subscriptions_per_stream() / 4;
  assert(max_length >= symbols_.size());
  return max_length - symbols_.size();
}

template <>
void MarketStream::subscribe_agg_trade(const std::vector<std::string> &symbols) {
  assert(symbols.empty() == false);
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}@aggTrade"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, R"(@aggTrade",")"sv),
      ++request_id_);
  connection_.send_text(message);
}

template <>
void MarketStream::subscribe_trade(const std::vector<std::string> &symbols) {
  assert(symbols.empty() == false);
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}@trade"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, R"(@trade",")"sv),
      ++request_id_);
  connection_.send_text(message);
}

template <>
void MarketStream::subscribe_mini_ticker(const std::vector<std::string> &symbols) {
  assert(symbols.empty() == false);
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}@miniTicker"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, R"(@miniTicker",")"sv),
      ++request_id_);
  connection_.send_text(message);
}

template <>
void MarketStream::subscribe_book_ticker(const std::vector<std::string> &symbols) {
  assert(symbols.empty() == false);
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}@bookTicker"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, R"(@bookTicker",")"sv),
      ++request_id_);
  connection_.send_text(message);
}

template <>
void MarketStream::subscribe_depth(const std::vector<std::string> &symbols) {
  assert(symbols.empty() == false);
  auto stream = fmt::format(
      R"(@depth{}@{}ms)"sv,
      Flags::ws_subscribe_depth_levels(),
      Flags::ws_subscribe_depth_freq_msecs());
  auto separator = fmt::format(R"({}",")"sv, stream);
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}{}"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, separator),
      stream,
      ++request_id_);
  connection_.send_text(message);
}

void MarketStream::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.parse, metrics::PROFILE)
      .write(profile_.error, metrics::PROFILE)
      .write(profile_.result, metrics::PROFILE)
      .write(profile_.agg_trade, metrics::PROFILE)
      .write(profile_.trade, metrics::PROFILE)
      .write(profile_.mini_ticker, metrics::PROFILE)
      .write(profile_.book_ticker, metrics::PROFILE)
      .write(profile_.depth, metrics::PROFILE)
      .write(profile_.depth_update, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void MarketStream::operator()(const core::web::Socket::Connected &) {
  // handler_(*this);
}

void MarketStream::operator()(const core::web::Socket::Disconnected &) {
  // handler_(*this);
  ++counter_.disconnect;
}

void MarketStream::operator()(const core::web::Socket::Ready &) {
  LOG(INFO)("Ready (#{})"sv, market_stream_id_);
  if (Flags::ws_subscribe_trade_details()) {
    subscribe_trade(symbols_);
  } else {
    subscribe_agg_trade(symbols_);
  }
  subscribe_mini_ticker(symbols_);
  subscribe_book_ticker(symbols_);
  subscribe_depth(symbols_);
}

void MarketStream::subscribe(const std::vector<std::string> &symbols) {
  assert(symbols.size() <= capacity());
  for (auto &symbol : symbols) {
#if !defined(NDEBUG)
    auto iter = std::find(symbols_.begin(), symbols_.end(), symbol);
    LOG_IF(FATAL, iter != symbols_.end())("Unexpected"sv);
#endif
    symbols_.emplace_back(symbol);
  }
  // only subscribe incremental symbols
  if (Flags::ws_subscribe_trade_details()) {
    subscribe_trade(symbols);
  } else {
    subscribe_agg_trade(symbols);
  }
  subscribe_mini_ticker(symbols);
  subscribe_book_ticker(symbols);
  subscribe_depth(symbols);
}

void MarketStream::operator()(const core::web::Socket::Close &) {
}

void MarketStream::operator()(const core::web::Socket::Latency &latency) {
  server::TraceInfo trace_info;
  ExternalLatency external_latency{
      .name = name_,
      .latency = latency.sample,
  };
  handler_(external_latency, trace_info);
  latency_.ping.update(latency.sample);
}

void MarketStream::operator()(const core::web::Socket::Text &text) {
  parse(text.payload);
}

void MarketStream::parse(const std::string_view &message) {
  profile_.parse([&]() {
    try {
      server::TraceInfo trace_info;
      core::json::Buffer buffer(decode_buffer_);
      json::MarketStreamParser::dispatch(*this, message, buffer, trace_info);
    } catch (std::exception &e) {
      LOG(WARNING)(R"(message="{}")"sv, message);
      LOG(FATAL)(R"(ERROR what="{}")"sv, e.what());
    }
  });
}

void MarketStream::operator()(int32_t id, const json::Error &error) {
  profile_.error([&]() { LOG(WARNING)(R"(id={}, error={})"sv, id, error); });
}

void MarketStream::operator()(int32_t id, const json::Result &result) {
  profile_.result([&]() { LOG(INFO)(R"(id={}, result={})"sv, id, result); });
}

void MarketStream::operator()(
    const json::AggTrade &agg_trade, const server::TraceInfo &trace_info) {
  profile_.agg_trade([&]() {
    VLOG(3)(R"(agg_trade={})"sv, agg_trade);
    handler_(agg_trade, trace_info);
  });
}

void MarketStream::operator()(const json::Trade &trade, const server::TraceInfo &trace_info) {
  profile_.trade([&]() {
    VLOG(3)(R"(trade={})"sv, trade);
    handler_(trade, trace_info);
  });
}

void MarketStream::operator()(
    const json::MiniTicker &mini_ticker, const server::TraceInfo &trace_info) {
  profile_.mini_ticker([&]() {
    VLOG(3)(R"(mini_ticker={})"sv, mini_ticker);
    handler_(mini_ticker, trace_info);
  });
}

void MarketStream::operator()(
    const json::BookTicker &book_ticker, const server::TraceInfo &trace_info) {
  profile_.book_ticker([&]() {
    VLOG(3)(R"(book_ticker={})"sv, book_ticker);
    handler_(book_ticker, trace_info);
  });
}

void MarketStream::operator()(
    const std::string_view &symbol, const json::Depth &depth, const server::TraceInfo &trace_info) {
  profile_.depth([&]() {
    VLOG(3)(R"(symbol="{}", depth={})"sv, symbol, depth);
    handler_(symbol, depth, trace_info);
  });
}

void MarketStream::operator()(
    const std::string_view &symbol,
    const json::DepthUpdate &depth_update,
    const server::TraceInfo &trace_info) {
  profile_.depth_update([&]() {
    VLOG(3)(R"(symbol="{}", depth_update={})"sv, symbol, depth_update);
    handler_(symbol, depth_update, trace_info);
  });
}

}  // namespace binance
}  // namespace roq
