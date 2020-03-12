/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <chrono>
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

class Gateway;

class WebSocket final
    : public core::web::Socket::Handler,
      public json::Parser::Handler {
 public:
  WebSocket(
      Gateway& gateway,
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

  void subscribe(const std::string_view& topic);

  void subscribe(
      const std::string_view& topic,
      const std::vector<std::string>& filter);

  void operator()(Metrics& metrics);

 protected:
  void operator()(const core::web::Socket::Connected&) override;
  void operator()(const core::web::Socket::Disconnected&) override;
  void operator()(const core::web::Socket::Ready&) override;
  void operator()(const core::web::Socket::Close&) override;
  void operator()(const core::web::Socket::Latency&) override;
  void operator()(const core::web::Socket::Text&) override;

  void parse(const std::string_view& message);
  void parse_helper(const std::string_view& message);

  void send_cancel_all_after();

 private:
  Gateway& _gateway;
  // authentication
  Random& _random;
  // web socket
  core::web::Socket _connection;
  // buffers
  core::utils::Buffer _decode_buffer;
  // session
  std::chrono::nanoseconds _next_heartbeat = {};
  std::chrono::nanoseconds _next_cancel_all_after = {};
  // metrics
  struct {
    core::metrics::Counter
      disconnect;
  } _counter;
  struct {
    core::metrics::Profile
      parse,
      cancel_all_after,
      error,
      execution,
      funding,
      handshake,
      instrument,
      liquidation,
      margin,
      order,
      order_book_l2,
      position,
      quote,
      settlement,
      subscribe,
      trade;
  } _profile;
  struct {
    core::metrics::Latency
      ping,
      heartbeat;
  } _latency;
};

}  // namespace binance
}  // namespace roq
