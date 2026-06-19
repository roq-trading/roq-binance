/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/download.hpp"
#include "roq/core/timer_queue.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/protocol/json/market_stream_parser.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct MarketData final : public web::socket::Client::Handler, public protocol::json::MarketStreamParser::Handler {
  struct Handler {};

  MarketData(Handler &, io::Context &, uint16_t stream_id, Priority, Shared &, size_t index);

  MarketData(MarketData const &) = delete;

  bool ready() const { return connection_status_ == ConnectionStatus::READY; }

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
  void operator()(ConnectionStatus, std::string_view const &reason = {});

  void subscribe(std::span<Symbol const> const &symbols);

  void subscribe(std::span<Symbol const> const &symbols, std::string_view const &channel);

  void parse(std::string_view const &message);

  // response
  void operator()(Trace<protocol::json::Error> const &, int64_t id) override;
  void operator()(Trace<protocol::json::Result> const &, int64_t id) override;

  // update
  void operator()(Trace<protocol::json::AggTrade> const &) override;
  void operator()(Trace<protocol::json::Trade> const &) override;
  void operator()(Trace<protocol::json::MiniTicker> const &) override;
  void operator()(Trace<protocol::json::BookTicker> const &) override;
  void operator()(Trace<protocol::json::Depth> const &, std::string_view const &symbol) override;
  void operator()(Trace<protocol::json::DepthUpdate> const &) override;

  void check_subscribe_queue(std::chrono::nanoseconds now);

  Handler &handler_;
  // config
  uint16_t const stream_id_;
  Priority const priority_;
  std::string const name_;
  size_t const index_;
  // web socket
  std::unique_ptr<web::socket::Client> connection_;
  // buffers
  core::json::BufferStack decode_buffer_;
  // session
  uint64_t request_id_ = {};
  // metrics
  struct {
    utils::metrics::Counter disconnect, total_bytes_received;
  } counter_;
  struct {
    utils::metrics::Profile parse, error, result, agg_trade, trade, mini_ticker, book_ticker, depth, depth_update;
  } profile_;
  struct {
    utils::metrics::Latency ping, heartbeat;
  } latency_;
  // cache
  Shared &shared_;
  // state
  ConnectionStatus connection_status_ = {};
  // queue
  core::TimerQueue<std::string> subscribe_queue_;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
