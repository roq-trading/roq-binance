/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>

#include "roq/utils/container.hpp"

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/gauge.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/download_2.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/server/stream.hpp"

#include "roq/binance/gateway/account.hpp"
#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/protocol/json/wsapi_parser.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct WebSocket final : public Base<WebSocket>,
                         public server::OrderActionStream,
                         public web::socket::Client::Handler,
                         public protocol::json::WSAPIParser::Handler {
  struct Handler {};

  WebSocket(Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, std::string_view const &interface = {});

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

  // server::OrderActionStream

  uint16_t operator()(Event<CreateOrder> const &, server::oms::Order const &, server::oms::RefData const &, std::string_view const &request_id) override;
  uint16_t operator()(
      Event<ModifyOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) override;
  uint16_t operator()(
      Event<CancelOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) override;
  uint16_t operator()(Event<CancelAllOrders> const &, std::string_view const &request_id) override;

  // web::socket::Client::Handler

  void operator()(Trace<web::socket::Connected> const &) override;
  void operator()(Trace<web::socket::Disconnected> const &) override;
  void operator()(Trace<web::socket::Ready> const &) override;
  void operator()(Trace<web::socket::Close> const &) override;
  void operator()(Trace<web::socket::Latency> const &) override;
  void operator()(Trace<web::socket::Text> const &) override;
  void operator()(Trace<web::socket::Binary> const &) override;

  // core::Download

  enum class State {
    UNDEFINED = 0,
    SESSION_LOGON,
    USER_DATA_STREAM_SUBSCRIBE,
    ACCOUNT_STATUS,
    OPEN_ORDERS_STATUS,
    MY_TRADES,
    DONE,
  };

  int32_t download(Trace<State> const &);

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

  void session_logon(TraceInfo const &);

  void user_data_stream_subscribe(TraceInfo const &);

  void account_status(TraceInfo const &);
  void open_orders_status(TraceInfo const &);
  void my_trades(TraceInfo const &);

  void order_place(Event<CreateOrder> const &, server::oms::Order const &, server::oms::RefData const &, std::string_view const &request_id);
  void order_amend_keep_priority(
      Event<ModifyOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  void order_cancel(
      Event<CancelOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  void open_orders_cancel_all(Event<CancelAllOrders> const &, std::string_view const &request_id);

  void parse(TraceInfo const &, std::string_view const &message);

  void operator()(Trace<protocol::json::OutboundAccountPositionData> const &);
  void operator()(Trace<protocol::json::BalanceUpdateData> const &);
  void operator()(Trace<protocol::json::ExecutionReportData> const &);

  void update_rate_limits(auto &event);

 private:
  [[maybe_unused]] Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // web socket
  std::unique_ptr<web::socket::Client> connection_;
  // buffers
  core::json::BufferStack decode_buffer_;
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile parse,                                 //
        session_logon,                                             //
        user_data_stream_subscribe,                                //
        account_status, open_orders_status, my_trades,             //
        order_place, order_place_ack,                              //
        order_amend_keep_priority, order_amend_keep_priority_ack,  //
        order_cancel, order_cancel_ack,                            //
        open_orders_cancel_all, open_orders_cancel_all_ack,        //
        outbound_account_position,                                 //
        balance_update,                                            //
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
  // experimental
  uint32_t request_id_;
  utils::unordered_set<std::string> open_orders_symbols_;
  std::string request_encode_buffer_;
  std::string encode_buffer_;
  // state
  bool ready_ = false;
  ConnectionStatus connection_status_ = {};
  core::Download2<State> download_;
  bool download_trades_is_first_ = true;  // note! lookback depends on it
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
