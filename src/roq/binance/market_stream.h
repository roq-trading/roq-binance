/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/event/base.h"
#include "roq/core/event/dns_base.h"

#include "roq/core/web/socket.h"

#include "roq/binance/random.h"

#include "roq/binance/json/market_stream_parser.h"

namespace roq {
namespace binance {

class MarketStream final
    : public core::web::Socket::Handler,
      public json::MarketStreamParser::Handler {
 public:
  struct Handler {
    virtual void operator()(const json::AggTrade&) = 0;
    virtual void operator()(const json::Trade&) = 0;
    virtual void operator()(const json::MiniTicker&) = 0;
    virtual void operator()(const json::BookTicker&) = 0;
    virtual void operator()(
        const std::string_view& symbol,
        const json::Depth& depth) = 0;
    virtual void operator()(
        const std::string_view& symbol,
        const json::DepthUpdate& depth_update) = 0;
  };
  MarketStream(
      Handler& handler,
      Random& random,
      core::event::Base& base,
      core::event::DNSBase& dns_base,
      core::ssl::Context& ssl_context,
      uint32_t market_stream_id,
      std::vector<std::string>&& symbols);

  MarketStream(MarketStream&&) = delete;
  MarketStream(const MarketStream&) = delete;

  bool ready() const;

  void operator()(const StartEvent&);
  void operator()(const StopEvent&);
  void operator()(const TimerEvent&);

  void operator()(Metrics& metrics);

  size_t capacity() const;

  void subscribe(const std::vector<std::string>& symbols);

 protected:
  template <typename T>
  void subscribe_agg_trade(const T& symbols);

  template <typename T>
  void subscribe_trade(const T& symbols);

  template <typename T>
  void subscribe_mini_ticker(const T& symbols);

  template <typename T>
  void subscribe_book_ticker(const T& symbols);

  template <typename T>
  void subscribe_depth(const T& symbols);

 protected:
  void operator()(const core::web::Socket::Connected&) override;
  void operator()(const core::web::Socket::Disconnected&) override;
  void operator()(const core::web::Socket::Ready&) override;
  void operator()(const core::web::Socket::Close&) override;
  void operator()(const core::web::Socket::Latency&) override;
  void operator()(const core::web::Socket::Text&) override;

  void parse(const std::string_view& message);

  // response
  void operator()(int32_t, const json::Error&) override;
  void operator()(int32_t, const json::Result&) override;

  // update
  void operator()(const json::AggTrade&) override;
  void operator()(const json::Trade&) override;
  void operator()(const json::MiniTicker&) override;
  void operator()(const json::BookTicker&) override;
  void operator()(
      const std::string_view& symbol,
      const json::Depth& depth) override;
  void operator()(
      const std::string_view& symbol,
      const json::DepthUpdate& depth_update) override;

 private:
  Handler& _handler;
  // config
  uint32_t _market_stream_id;
  std::vector<std::string> _symbols;
  // authentication
  Random& _random;
  // web socket
  core::web::Socket _connection;
  // buffers
  core::utils::Buffer _decode_buffer;
  // session
  uint64_t _request_id = 0;
  // metrics
  struct {
    core::metrics::Counter
      disconnect;
  } _counter;
  struct {
    core::metrics::Profile
      parse,
      error,
      result,
      agg_trade,
      trade,
      mini_ticker,
      book_ticker,
      depth,
      depth_update;
  } _profile;
  struct {
    core::metrics::Latency
      ping,
      heartbeat;
  } _latency;
};

}  // namespace binance
}  // namespace roq
