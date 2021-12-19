/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/io/context.h"

#include "roq/core/web/client_socket.h"

#include "roq/download.h"
#include "roq/server.h"

#include "roq/binance/shared.h"

#include "roq/binance/json/market_stream_parser.h"

namespace roq {
namespace binance {

class MarketData final : public core::web::ClientSocket::Handler,
                         public json::MarketStreamParser::Handler {
 public:
  struct Handler {
    virtual void operator()(const server::Trace<StreamStatus> &) = 0;
    virtual void operator()(const server::Trace<ExternalLatency> &) = 0;
    virtual void operator()(const server::Trace<TopOfBook> &, bool is_last) = 0;
    virtual void operator()(
        const server::Trace<MarketByPriceUpdate> &, bool is_last, bool refresh) = 0;
    virtual void operator()(const server::Trace<TradeSummary> &, bool is_last) = 0;
    virtual void operator()(const server::Trace<StatisticsUpdate> &, bool is_last) = 0;
  };

  MarketData(Handler &, core::io::Context &, uint32_t stream_id, Shared &, size_t index);

  MarketData(MarketData &&) = delete;
  MarketData(const MarketData &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &);

  void subscribe(size_t start_from = 0);

 protected:
  void operator()(const core::web::ClientSocket::Connected &) override;
  void operator()(const core::web::ClientSocket::Disconnected &) override;
  void operator()(const core::web::ClientSocket::Ready &) override;
  void operator()(const core::web::ClientSocket::Close &) override;
  void operator()(const core::web::ClientSocket::Latency &) override;
  void operator()(const core::web::ClientSocket::Text &) override;
  void operator()(const core::web::ClientSocket::Binary &) override;

 private:
  void operator()(ConnectionStatus);

  void subscribe(const roq::span<std::string const> &symbols);

  void subscribe_agg_trade(const roq::span<std::string const> &symbols);
  void subscribe_trade(const roq::span<std::string const> &symbols);
  void subscribe_mini_ticker(const roq::span<std::string const> &symbols);
  void subscribe_book_ticker(const roq::span<std::string const> &symbols);
  void subscribe_diff_depth(const roq::span<std::string const> &symbols);

  void parse(const std::string_view &message);

  // response
  void operator()(const server::Trace<json::Error> &, int32_t id) override;
  void operator()(const server::Trace<json::Result> &, int32_t id) override;

  // update
  void operator()(const server::Trace<json::AggTrade> &) override;
  void operator()(const server::Trace<json::Trade> &) override;
  void operator()(const server::Trace<json::MiniTicker> &) override;
  void operator()(const server::Trace<json::BookTicker> &) override;
  void operator()(const server::Trace<json::Depth> &, const std::string_view &symbol) override;
  void operator()(const server::Trace<json::DepthUpdate> &) override;

  void check_subscribe_queue(std::chrono::nanoseconds now);

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  const size_t index_;
  // web socket
  core::web::ClientSocket connection_;
  // buffers
  core::Buffer decode_buffer_;
  // session
  uint64_t request_id_ = {};
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
  // cache
  Shared &shared_;
  // state
  ConnectionStatus status_ = {};
  // queue
  std::deque<std::pair<std::chrono::nanoseconds, std::string> > subscribe_queue_;
};

}  // namespace binance
}  // namespace roq
