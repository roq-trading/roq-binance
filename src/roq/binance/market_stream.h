/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/io/context.h"

#include "roq/core/web/socket.h"

#include "roq/server.h"

#include "roq/binance/json/market_stream_parser.h"

namespace roq {
namespace binance {

class MarketStream final : public core::web::Socket::Handler,
                           public json::MarketStreamParser::Handler {
 public:
  struct Handler {
    virtual void operator()(const ExternalLatency &, const server::TraceInfo &) = 0;
    virtual void operator()(const json::AggTrade &, const server::TraceInfo &) = 0;
    virtual void operator()(const json::Trade &, const server::TraceInfo &) = 0;
    virtual void operator()(const json::MiniTicker &, const server::TraceInfo &) = 0;
    virtual void operator()(const json::BookTicker &, const server::TraceInfo &) = 0;
    virtual void operator()(
        const std::string_view &symbol, const json::Depth &depth, const server::TraceInfo &) = 0;
    virtual void operator()(
        const std::string_view &symbol,
        const json::DepthUpdate &depth_update,
        const server::TraceInfo &) = 0;
  };
  MarketStream(
      Handler &handler,
      core::io::Context &context,
      uint32_t market_stream_id,
      std::vector<std::string> &&symbols);

  MarketStream(MarketStream &&) = delete;
  MarketStream(const MarketStream &) = delete;

  bool ready() const;

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &writer);

  size_t capacity() const;

  void subscribe(const std::vector<std::string> &symbols);

 protected:
  template <typename T>
  void subscribe_agg_trade(const T &symbols);

  template <typename T>
  void subscribe_trade(const T &symbols);

  template <typename T>
  void subscribe_mini_ticker(const T &symbols);

  template <typename T>
  void subscribe_book_ticker(const T &symbols);

  template <typename T>
  void subscribe_depth(const T &symbols);

 protected:
  void operator()(const core::web::Socket::Connected &) override;
  void operator()(const core::web::Socket::Disconnected &) override;
  void operator()(const core::web::Socket::Ready &) override;
  void operator()(const core::web::Socket::Close &) override;
  void operator()(const core::web::Socket::Latency &) override;
  void operator()(const core::web::Socket::Text &) override;

  void parse(const std::string_view &message);

  // response
  void operator()(int32_t, const json::Error &) override;
  void operator()(int32_t, const json::Result &) override;

  // update
  void operator()(const json::AggTrade &, const server::TraceInfo &) override;
  void operator()(const json::Trade &, const server::TraceInfo &) override;
  void operator()(const json::MiniTicker &, const server::TraceInfo &) override;
  void operator()(const json::BookTicker &, const server::TraceInfo &) override;
  void operator()(
      const std::string_view &symbol, const json::Depth &depth, const server::TraceInfo &) override;
  void operator()(
      const std::string_view &symbol,
      const json::DepthUpdate &depth_update,
      const server::TraceInfo &) override;

 private:
  Handler &handler_;
  // config
  const uint32_t market_stream_id_;
  std::vector<std::string> symbols_;
  const std::string name_;
  // web socket
  core::web::Socket connection_;
  // buffers
  core::utils::Buffer decode_buffer_;
  // session
  uint64_t request_id_ = 0;
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile parse, error, result, agg_trade, trade, mini_ticker, book_ticker, depth,
        depth_update;
  } profile_;
  struct {
    core::metrics::Latency ping, heartbeat;
  } latency_;
};

}  // namespace binance
}  // namespace roq
