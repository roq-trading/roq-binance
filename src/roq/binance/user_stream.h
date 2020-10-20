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

#include "roq/server.h"

#include "roq/binance/random.h"

#include "roq/binance/json/user_stream_parser.h"

namespace roq {
namespace binance {

class UserStream final : public core::web::Socket::Handler,
                         public json::UserStreamParser::Handler {
 public:
  struct Handler {
    virtual void operator()(
        const json::OutboundAccountInfo &, const server::TraceInfo &) = 0;
    virtual void operator()(
        const json::OutboundAccountPosition &, const server::TraceInfo &) = 0;
    virtual void operator()(
        const json::BalanceUpdate &, const server::TraceInfo &) = 0;
    virtual void operator()(
        const json::ExecutionReport &, const server::TraceInfo &) = 0;
  };
  UserStream(
      Handler &handler,
      Random &random,
      core::event::Base &base,
      core::event::DNSBase &dns_base,
      core::ssl::Context &ssl_context,
      const std::string_view &listen_key);

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

  void operator()(
      const json::OutboundAccountInfo &, const server::TraceInfo &) override;
  void operator()(
      const json::OutboundAccountPosition &,
      const server::TraceInfo &) override;
  void operator()(
      const json::BalanceUpdate &, const server::TraceInfo &) override;
  void operator()(
      const json::ExecutionReport &, const server::TraceInfo &) override;

 private:
  Handler &_handler;
  // config
  const std::string _query;
  // authentication
  Random &_random;
  // web socket
  core::web::Socket _connection;
  // buffers
  core::utils::Buffer _decode_buffer;
  // session
  uint64_t _request_id = 0;
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } _counter;
  struct {
    core::metrics::Profile parse, outbound_account_info,
        outbound_account_position, balance_update, execution_report;
  } _profile;
  struct {
    core::metrics::Latency ping, heartbeat;
  } _latency;
};

}  // namespace binance
}  // namespace roq
