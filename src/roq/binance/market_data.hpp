/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "roq/core/download.hpp"
#include "roq/core/timer_queue.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/server.hpp"

#include "roq/binance/shared.hpp"

#include "roq/binance/json/market_stream_parser.hpp"

namespace roq {
namespace binance {

class MarketData final : public web::socket::Client::Handler, public json::MarketStreamParser::Handler {
 public:
  struct Handler {
    virtual void operator()(Trace<StreamStatus const> const &) = 0;
    virtual void operator()(Trace<ExternalLatency const> const &) = 0;
    virtual void operator()(Trace<TopOfBook const> const &, bool is_last) = 0;
    virtual void operator()(Trace<MarketByPriceUpdate const> const &, bool is_last, bool refresh) = 0;
    virtual void operator()(Trace<TradeSummary const> const &, bool is_last) = 0;
    virtual void operator()(Trace<StatisticsUpdate const> const &, bool is_last) = 0;
  };

  MarketData(Handler &, io::Context &, uint32_t stream_id, Shared &, size_t index);

  MarketData(MarketData &&) = delete;
  MarketData(MarketData const &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &);

  void subscribe(size_t start_from = 0);

 protected:
  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

 private:
  void operator()(ConnectionStatus);

  void subscribe(std::span<Symbol const> const &symbols);

  void subscribe(std::span<Symbol const> const &symbols, std::string_view const &channel);

  void parse(std::string_view const &message);

  // response
  void operator()(Trace<json::Error const> const &, int32_t id) override;
  void operator()(Trace<json::Result const> const &, int32_t id) override;

  // update
  void operator()(Trace<json::AggTrade const> const &) override;
  void operator()(Trace<json::Trade const> const &) override;
  void operator()(Trace<json::MiniTicker const> const &) override;
  void operator()(Trace<json::BookTicker const> const &) override;
  void operator()(Trace<json::Depth const> const &, std::string_view const &symbol) override;
  void operator()(Trace<json::DepthUpdate const> const &) override;

  void check_subscribe_queue(std::chrono::nanoseconds now);

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  const size_t index_;
  // web socket
  std::unique_ptr<web::socket::Client> connection_;
  // buffers
  core::Buffer decode_buffer_;
  // session
  uint64_t request_id_ = {};
  // metrics
  struct {
    core::metrics::Counter disconnect, total_bytes_received;
  } counter_;
  struct {
    core::metrics::Profile parse, error, result, agg_trade, trade, mini_ticker, book_ticker, depth, depth_update;
  } profile_;
  struct {
    core::metrics::Latency ping, heartbeat;
  } latency_;
  // cache
  Shared &shared_;
  // state
  ConnectionStatus status_ = {};
  // queue
  core::TimerQueue subscribe_queue_;
};

}  // namespace binance
}  // namespace roq
