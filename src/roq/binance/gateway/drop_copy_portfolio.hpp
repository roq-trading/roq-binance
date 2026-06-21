/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/download.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/binance/gateway/account.hpp"
#include "roq/binance/gateway/drop_copy.hpp"
#include "roq/binance/gateway/request.hpp"
#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/protocol/json/user_stream_parser.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct DropCopyPortfolio final : public DropCopy, public web::socket::Client::Handler, public protocol::json::UserStreamParser::Handler {
  DropCopyPortfolio(DropCopy::Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &, std::string_view const &listen_key, MarginMode);

 protected:
  // DropCopy

  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(metrics::Writer &) const override;

  // web::socket::Client::Handler

  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

  // helpers

  std::string_view get_query() const override { return query_; }

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  enum class State {
    UNDEFINED = 0,
    ACCOUNT,
    ORDERS,
    TRADES,
    DONE,
  };

  uint32_t download(State);

  void parse(std::string_view const &message);

  // protocol::json::UserStreamParser::Handler

  void operator()(Trace<protocol::json::OutboundAccountPosition> const &) override;
  void operator()(Trace<protocol::json::BalanceUpdate> const &) override;
  void operator()(Trace<protocol::json::ExecutionReport> const &) override;
  void operator()(Trace<protocol::json::ListStatus> const &) override;

  // helpers

  void request_account();
  void check_response_account();

  void request_orders();
  void check_response_orders();

  void request_trades();
  void check_response_trades();

 private:
  [[maybe_unused]] DropCopy::Handler &handler_;
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
  core::Download<State> download_;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
