/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/download.hpp"

#include "roq/server.hpp"

#include "roq/binance/account.hpp"
#include "roq/binance/drop_copy.hpp"
#include "roq/binance/drop_copy_state.hpp"
#include "roq/binance/request.hpp"
#include "roq/binance/shared.hpp"

#include "roq/binance/json/user_stream_parser.hpp"

namespace roq {
namespace binance {

struct DropCopyPortfolio final : public DropCopy, public web::socket::Client::Handler, public json::UserStreamParser::Handler {
  DropCopyPortfolio(DropCopy::Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &, std::string_view const &listen_key);

  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(metrics::Writer &) override;

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

  uint32_t download(DropCopyState);

  void parse(std::string_view const &message);

  void operator()(Trace<json::OutboundAccountPosition> const &) override;
  void operator()(Trace<json::BalanceUpdate> const &) override;
  void operator()(Trace<json::ExecutionReport> const &) override;
  void operator()(Trace<json::ListStatus> const &) override;

  void request_account();
  void check_response_account();

  void request_orders();
  void check_response_orders();

  void request_trades();
  void check_response_trades();

  DropCopy::Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // web socket
  std::unique_ptr<web::socket::Client> connection_;
  // buffers
  std::vector<std::byte> decode_buffer_;
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
  ConnectionStatus status_ = {};
  core::Download<DropCopyState> download_;
};

}  // namespace binance
}  // namespace roq
