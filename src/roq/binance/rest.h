/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/core/promise.h"

#include "roq/core/utils/buffer.h"

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/io/context.h"

#include "roq/core/web/client.h"

#include "roq/server.h"

#include "roq/binance/config.h"
#include "roq/binance/security.h"

#include "roq/binance/json/cancel_order.h"
#include "roq/binance/json/new_order.h"

namespace roq {
namespace binance {

class Rest final : public core::web::Client::Handler {
 public:
  struct Handler {
    virtual void operator()(const Rest &) = 0;
    virtual void operator()(const ExternalLatency &, const server::TraceInfo &) = 0;
  };

  Rest(Handler &handler, const Config &config, Security &security, core::io::Context &context);

  Rest(Rest &&) = delete;
  Rest(const Rest &) = delete;

  bool ready() const;

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &writer);

  template <typename T>
  void get(std::function<void(const core::Promise<T> &)> &&callback);

  void create_order(
      const CreateOrder &create_order,
      const std::string_view &cl_ord_id,
      std::function<void(const core::Promise<json::NewOrder> &)> &&callback);

  void cancel_order(
      const CancelOrder &cancel_order,
      const std::string_view &request_id,
      const server::OMS_Order &order,
      std::function<void(const core::Promise<json::CancelOrder> &)> &&callback);

 protected:
  void operator()(const core::web::Client::Connected &);
  void operator()(const core::web::Client::Disconnected &);
  void operator()(const core::web::Client::Latency &);

 private:
  Handler &handler_;
  // security
  Security &security_;
  // connection
  core::web::Client connection_;
  // buffers
  core::utils::Buffer decode_buffer_;
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile exchange_info, account, listen_key, depth, new_order, cancel_order;
  } profile_;
  struct {
    core::metrics::Latency ping;
  } latency_;
};

}  // namespace binance
}  // namespace roq
