/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "roq/utils/container.hpp"

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
#include "roq/binance/gateway/drop_copy_state_margin.hpp"
#include "roq/binance/gateway/order_entry.hpp"
#include "roq/binance/gateway/request.hpp"
#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/json/wsapi_parser.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct DropCopyMargin final : public DropCopy, public web::socket::Client::Handler, public json::WSAPIParser::Handler {
  DropCopyMargin(DropCopy::Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &, OrderEntry::ListenKeyUpdate const &);

  DropCopyMargin(DropCopyMargin const &) = delete;

  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(metrics::Writer &) const override;

  void operator()(OrderEntry::ListenKeyUpdate const &);

 protected:
  bool ready() const { return connection_status_ == ConnectionStatus::READY; }

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  uint32_t download(DropCopyStateMargin);

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

  void parse(std::string_view const &message);

  // json::WSAPIParser::Handler

  void operator()(Trace<json::WSAPISessionLogon> const &) override;
  //
  void operator()(Trace<json::WSAPIUserDataStreamSubscribe> const &) override;
  void operator()(Trace<json::WSAPIEventStreamTerminated> const &) override;
  //
  void operator()(Trace<json::WSAPIAccount> const &) override;
  void operator()(Trace<json::WSAPIOpenOrders> const &) override;
  void operator()(Trace<json::WSAPITrades> const &) override;
  //
  void operator()(Trace<json::WSAPIOrderPlace> const &, json::WSAPIRequest const &) override;
  void operator()(Trace<json::WSAPIOrderAmendKeepPriority> const &, json::WSAPIRequest const &) override;
  void operator()(Trace<json::WSAPICancelOrder> const &, json::WSAPIRequest const &) override;
  void operator()(Trace<json::WSAPICancelOpenOrders> const &, json::WSAPIRequest const &) override;
  //
  void operator()(Trace<json::WSAPIOutboundAccountPosition> const &) override;
  void operator()(Trace<json::WSAPIBalanceUpdate> const &) override;
  void operator()(Trace<json::WSAPIExecutionReport> const &) override;

  // helpers

  void operator()(Trace<json::OutboundAccountPositionData> const &);
  void operator()(Trace<json::BalanceUpdateData> const &);
  void operator()(Trace<json::ExecutionReportData> const &);

  void update_rate_limits(auto &event);

  template <typename... Args>
  void operator()(Trace<server::oms::Response> const &, uint8_t user_id, uint64_t order_id, Args &&...args);

  void operator()(Trace<server::oms::OrderUpdate> const &, std::string_view const &client_order_id);

  void check_response_listen_key();
  void check_response_account();
  void check_response_orders();

  DropCopy::Handler &handler_;
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
  core::Download<DropCopyStateMargin> download_;
  //
  bool download_listen_token_ = false;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
