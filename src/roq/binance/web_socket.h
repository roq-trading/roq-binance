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

#include "roq/binance/config.h"
#include "roq/binance/random.h"

#include "roq/binance/json/parser.h"

namespace roq {
namespace binance {

class WebSocket final
    : public core::web::Socket::Handler,
      public json::Parser::Handler {
 public:
  struct Handler {
    virtual void operator()(const WebSocket&) = 0;
    virtual void operator()(const json::Trade&) = 0;
    virtual void operator()(const json::BookTicker&) = 0;
    virtual void operator()(
        const std::string_view& symbol,
        const json::Depth& depth) = 0;
    virtual void operator()(
        const std::string_view& symbol,
        const json::DepthUpdate& depth_update) = 0;
  };
  WebSocket(
      Handler& handler,
      const Config& config,
      Random& random,
      core::event::Base& base,
      core::event::DNSBase& dns_base,
      core::ssl::Context& ssl_context);

  WebSocket(WebSocket&&) = delete;
  WebSocket(const WebSocket&) = delete;

  bool ready() const;

  void operator()(const StartEvent&);
  void operator()(const StopEvent&);
  void operator()(const TimerEvent&);

  void subscribe();

  void operator()(Metrics& metrics);

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
  void operator()(const json::Trade&) override;
  void operator()(const json::BookTicker&) override;
  void operator()(
      const std::string_view& symbol,
      const json::Depth& depth) override;
  void operator()(
      const std::string_view& symbol,
      const json::DepthUpdate& depth_update) override;

 private:
  Handler& _handler;
  // authentication
  Random& _random;
  // web socket
  core::web::Socket _connection;
  // buffers
  core::utils::Buffer _decode_buffer;
  // session
  std::chrono::nanoseconds _next_cancel_all_after = {};
  // metrics
  struct {
    core::metrics::Counter
      disconnect;
  } _counter;
  struct {
    core::metrics::Profile
      parse,
      trade,
      ticker,
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
