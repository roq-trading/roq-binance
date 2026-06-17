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

#include "roq/server/cache/cancel_order_request.hpp"

#include "roq/binance/gateway/account.hpp"
#include "roq/binance/gateway/order_entry.hpp"
#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/protocol/json/wsapi_parser.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct WebSocket final : public OrderEntry, public web::socket::Client::Handler, public protocol::json::WSAPIParser::Handler {
  WebSocket(OrderEntry::Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, std::string_view const &interface = {});

  WebSocket(WebSocket const &) = delete;

 protected:
  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(metrics::Writer &) const override;

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

  bool ready() const { return connection_status_ == ConnectionStatus::READY; }

  void session_logon();

  void user_data_stream_subscribe();

  void account_status();
  void open_orders_status();
  void my_trades();

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

  // web::socket::Client::Handler

  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  enum class State {
    UNDEFINED = 0,
    SESSION_LOGON,
    USER_DATA_STREAM_SUBSCRIBE,
    ACCOUNT_STATUS,
    OPEN_ORDERS_STATUS,
    MY_TRADES,
    DONE,
  };

  uint32_t download(State);

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

  template <typename... Args>
  void operator()(Trace<server::oms::Response> const &, uint8_t user_id, uint64_t order_id, Args &&...args);

  void operator()(Trace<server::oms::OrderUpdate> const &);

  OrderEntry::Handler &handler_;
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
  core::Download<State> download_;
  bool download_trades_is_first_ = true;  // note! lookback depends on it
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
