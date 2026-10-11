/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/download_2.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/server/stream.hpp"

#include "roq/binance/gateway/account.hpp"
#include "roq/binance/gateway/request.hpp"
#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/protocol/json/user_stream_parser.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct DropCopyPortfolio final : public Base<DropCopyPortfolio>,
                                 public server::Stream,
                                 public web::socket::Client::Handler,
                                 public protocol::json::UserStreamParser::Handler {
  struct Handler {};

  DropCopyPortfolio(Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &, std::string_view const &listen_key, MarginMode);

  // protected:
  friend base_type;

  // server::Stream

  uint16_t stream_id() const override { return stream_id_; }

  bool ready() const override { return connection_status_ == ConnectionStatus::READY; }

  void operator()(Trace<Start> const &) override;
  void operator()(Trace<Stop> const &) override;
  void operator()(Trace<Timer> const &) override;

  void operator()(metrics::Writer &) const override;

  void operator()(Trace<ConnectionStatus> const &, std::string_view const &reason = {}) override;

  // web::socket::Client::Handler

  void operator()(Trace<web::socket::Connected> const &) override;
  void operator()(Trace<web::socket::Disconnected> const &) override;
  void operator()(Trace<web::socket::Ready> const &) override;
  void operator()(Trace<web::socket::Close> const &) override;
  void operator()(Trace<web::socket::Latency> const &) override;
  void operator()(Trace<web::socket::Text> const &) override;
  void operator()(Trace<web::socket::Binary> const &) override;
  //
  std::string_view get_query() const override { return query_; }

  // core::Download

  enum class State {
    UNDEFINED = 0,
    ACCOUNT,
    ORDERS,
    TRADES,
    DONE,
  };

  int32_t download(Trace<State> const &);

  // protocol::json::UserStreamParser::Handler

  void operator()(Trace<protocol::json::OutboundAccountPosition> const &) override;
  void operator()(Trace<protocol::json::BalanceUpdate> const &) override;
  void operator()(Trace<protocol::json::ExecutionReport> const &) override;
  void operator()(Trace<protocol::json::ListStatus> const &) override;

  // helpers

  void request_account();
  void check_response_account(TraceInfo const &);

  void request_orders();
  void check_response_orders(TraceInfo const &);

  void request_trades();
  void check_response_trades(TraceInfo const &);

  void parse(TraceInfo const &, std::string_view const &message);

 private:
  [[maybe_unused]] Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // web socket
  std::string query_;
  std::unique_ptr<web::socket::Client> connection_;
  // buffers
  core::json::BufferStack decode_buffer_;
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile parse, outbound_account_position, balance_update, execution_report, list_status;
  } profile_;
  struct {
    utils::metrics::Latency ping, heartbeat;
  } latency_;
  // accounts
  Account &account_;
  // shared
  Shared &shared_;
  Request &request_;
  // state
  bool ready_ = false;
  ConnectionStatus connection_status_ = {};
  core::Download2<State> download_;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
