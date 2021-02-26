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

#include "roq/binance/json/user_stream_parser.h"

namespace roq {
namespace binance {

class UserStream final : public core::web::Socket::Handler, public json::UserStreamParser::Handler {
 public:
  struct Handler {
    virtual void operator()(const ExternalLatency &, const server::TraceInfo &) = 0;
    virtual void operator()(const json::OutboundAccountInfo &, const server::TraceInfo &) = 0;
    virtual void operator()(const json::OutboundAccountPosition &, const server::TraceInfo &) = 0;
    virtual void operator()(const json::BalanceUpdate &, const server::TraceInfo &) = 0;
    virtual void operator()(const json::ExecutionReport &, const server::TraceInfo &) = 0;
  };
  UserStream(Handler &handler, core::io::Context &context, const std::string_view &listen_key);

  UserStream(UserStream &&) = delete;
  UserStream(const UserStream &) = delete;

  bool ready() const;

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &writer);

 protected:
  void operator()(const core::web::Socket::Connected &) override;
  void operator()(const core::web::Socket::Disconnected &) override;
  void operator()(const core::web::Socket::Ready &) override;
  void operator()(const core::web::Socket::Close &) override;
  void operator()(const core::web::Socket::Latency &) override;
  void operator()(const core::web::Socket::Text &) override;

  void parse(const std::string_view &message);

  void operator()(const json::OutboundAccountInfo &, const server::TraceInfo &) override;
  void operator()(const json::OutboundAccountPosition &, const server::TraceInfo &) override;
  void operator()(const json::BalanceUpdate &, const server::TraceInfo &) override;
  void operator()(const json::ExecutionReport &, const server::TraceInfo &) override;

 private:
  Handler &handler_;
  // web socket
  core::web::Socket connection_;
  // buffers
  core::utils::Buffer decode_buffer_;
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile parse, outbound_account_info, outbound_account_position, balance_update,
        execution_report;
  } profile_;
  struct {
    core::metrics::Latency ping, heartbeat;
  } latency_;
};

}  // namespace binance
}  // namespace roq
