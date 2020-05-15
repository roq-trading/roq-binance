/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/promise.h"

#include "roq/core/utils/buffer.h"

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/ssl/ssl.h"

#include "roq/core/event/base.h"
#include "roq/core/event/dns_base.h"

#include "roq/core/web/client.h"

#include "roq/binance/config.h"
#include "roq/binance/random.h"

namespace roq {
namespace binance {

class Rest final
    : public core::web::Client::Handler {
 public:
  struct Handler {
    virtual void operator()(const Rest&) = 0;
  };

  Rest(
      Handler& handler,
      const Config& config,
      Random& random,
      core::event::Base& base,
      core::event::DNSBase& dns_base,
      core::ssl::Context& ssl_context);

  Rest(Rest&&) = delete;
  Rest(const Rest&) = delete;

  bool ready() const;

  void operator()(const StartEvent&);
  void operator()(const StopEvent&);
  void operator()(const TimerEvent&);

  void operator()(Metrics& metrics);

  template <typename T>
  void get(std::function<void(const core::Promise<T>&)>&& callback);

 protected:
  void operator()(const core::web::Client::Connected&);
  void operator()(const core::web::Client::Disconnected&);
  void operator()(const core::web::Client::Latency&);

 private:
  Handler& _handler;
  // authentication
  Random& _random;
  // connection
  core::web::Client _connection;
  // buffers
  core::utils::Buffer _decode_buffer;
  // metrics
  struct {
    core::metrics::Counter
      disconnect;
  } _counter;
  struct {
    core::metrics::Profile
      exchange_info;
  } _profile;
  struct {
    core::metrics::Latency
      ping;
  } _latency;
};

}  // namespace binance
}  // namespace roq
