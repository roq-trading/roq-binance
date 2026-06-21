/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/gauge.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/download.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/binance/gateway/account.hpp"
#include "roq/binance/gateway/drop_copy.hpp"
#include "roq/binance/gateway/order_entry.hpp"
#include "roq/binance/gateway/request.hpp"
#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/protocol/json/wsapi_parser.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct DropCopyMargin final : public DropCopy, public web::socket::Client::Handler, public protocol::json::WSAPIParser::Handler {
  DropCopyMargin(DropCopy::Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &, OrderEntry::ListenKeyUpdate const &);

  DropCopyMargin(DropCopyMargin const &) = delete;

  // cross-communication

  void operator()(OrderEntry::ListenKeyUpdate const &);

  // DropCopy

  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(metrics::Writer &) const override;

 protected:
  // helpers

  bool ready() const { return connection_status_ == ConnectionStatus::READY; }

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  enum class State {
    UNDEFINED = 0,
    SESSION_LOGON,
    USER_DATA_STREAM_SUBSCRIBE,
    ACCOUNT,
    ORDERS,
    DONE,
  };

  uint32_t download(State);

  void session_logon();

  void subscribe_user_data_stream();

  void get_account();
  void get_orders();

  // web::socket::Client::Handler

  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

  // helpers

  void parse(std::string_view const &message);

  // protocol::json::WSAPIParser::Handler

  void operator()(Trace<protocol::json::WSAPISessionLogon> const &) override;
  //
  void operator()(Trace<protocol::json::WSAPIUserDataStreamSubscribe> const &) override;
  void operator()(Trace<protocol::json::WSAPIEventStreamTerminated> const &) override;
  //
  void operator()(Trace<protocol::json::WSAPIAccount> const &) override;
  void operator()(Trace<protocol::json::WSAPIOpenOrders> const &) override;
  void operator()(Trace<protocol::json::WSAPITrades> const &) override;
  //
  void operator()(Trace<protocol::json::WSAPIOrderPlace> const &, protocol::json::WSAPIRequest const &) override;
  void operator()(Trace<protocol::json::WSAPIOrderAmendKeepPriority> const &, protocol::json::WSAPIRequest const &) override;
  void operator()(Trace<protocol::json::WSAPICancelOrder> const &, protocol::json::WSAPIRequest const &) override;
  void operator()(Trace<protocol::json::WSAPICancelOpenOrders> const &, protocol::json::WSAPIRequest const &) override;
  //
  void operator()(Trace<protocol::json::WSAPIOutboundAccountPosition> const &) override;
  void operator()(Trace<protocol::json::WSAPIBalanceUpdate> const &) override;
  void operator()(Trace<protocol::json::WSAPIExecutionReport> const &) override;

  // helpers

  void operator()(Trace<protocol::json::OutboundAccountPositionData> const &);
  void operator()(Trace<protocol::json::BalanceUpdateData> const &);
  void operator()(Trace<protocol::json::ExecutionReportData> const &);

  void update_rate_limits(auto &event);

  void check_response_listen_key();
  void check_response_account();
  void check_response_orders();

 private:
  [[maybe_unused]] DropCopy::Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  std::string listen_token_;
  MarginMode const margin_mode_;
  // web socket
  std::unique_ptr<web::socket::Client> connection_;
  // buffers
  core::json::BufferStack decode_buffer_;
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile parse,   //
        session_logon,               //
        user_data_stream_subscribe,  //
        outbound_account_position,   //
        balance_update,              //
        execution_report;
  } profile_;
  struct {
    utils::metrics::Latency ping, heartbeat;
  } latency_;
  struct {
    utils::metrics::Gauge request_weight_1m, create_order_10s, create_order_1d;
  } rate_limiter_;
  // authentication
  Account &account_;
  // shared
  Shared &shared_;
  Request &request_;
  // experimental
  uint32_t request_id_;
  std::string request_encode_buffer_;
  // state
  bool ready_ = false;
  ConnectionStatus connection_status_ = {};
  core::Download<State> download_;
  //
  bool download_listen_token_ = false;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
