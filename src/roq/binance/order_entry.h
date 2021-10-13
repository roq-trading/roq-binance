/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/core/buffer.h"

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/io/context.h"

#include "roq/core/web/client.h"

#include "roq/download.h"
#include "roq/server.h"

#include "roq/binance/order_entry_state.h"
#include "roq/binance/security.h"
#include "roq/binance/shared.h"

#include "roq/binance/json/account.h"
#include "roq/binance/json/cancel_order.h"
#include "roq/binance/json/listen_key.h"
#include "roq/binance/json/new_order.h"
#include "roq/binance/json/open_orders.h"

namespace roq {
namespace binance {

class OrderEntry final : public core::web::Client::Handler {
 public:
  struct ListenKeyUpdate final {
    std::string_view account;
    std::string_view listen_key;
  };

  struct Handler {
    virtual void operator()(const server::Trace<StreamStatus> &) = 0;
    virtual void operator()(const server::Trace<ExternalLatency> &) = 0;
    virtual void operator()(const server::Trace<ReferenceData> &, bool is_last) = 0;
    virtual void operator()(const server::Trace<MarketStatus> &, bool is_last) = 0;
    virtual void operator()(const server::Trace<FundsUpdate> &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(const ListenKeyUpdate &) = 0;
  };

  OrderEntry(Handler &, core::io::Context &, uint16_t stream_id, Security &, Shared &);

  OrderEntry(OrderEntry &&) = delete;
  OrderEntry(const OrderEntry &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &);

  uint16_t operator()(
      const Event<CreateOrder> &, const oms::Order &, const std::string_view &request_id);
  uint16_t operator()(
      const Event<ModifyOrder> &,
      const oms::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id);
  uint16_t operator()(
      const Event<CancelOrder> &,
      const oms::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id);
  uint16_t operator()(const Event<CancelAllOrders> &, const std::string_view &request_id);

 protected:
  void operator()(const core::web::Client::Connected &);
  void operator()(const core::web::Client::Disconnected &);
  void operator()(const core::web::Client::Latency &);

  void operator()(ConnectionStatus);

  uint32_t download(OrderEntryState state);

  void get_listen_key();
  void get_listen_key_ack(const core::web::Response &);
  void operator()(const server::Trace<json::ListenKey> &);

  void get_account();
  void get_account_ack(const core::web::Response &);
  void operator()(const server::Trace<json::Account> &);

  void get_open_orders();
  void get_open_orders_ack(const core::web::Response &);
  void operator()(const server::Trace<json::OpenOrders> &);

  void refresh_listen_key();

  void new_order(const CreateOrder &, const std::string_view &cl_ord_id);
  void new_order_ack(const core::web::Response &);
  void operator()(const server::Trace<json::NewOrder> &);

  void cancel_order(
      const CancelOrder &,
      const oms::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id);
  void cancel_order_ack(const core::web::Response &);
  void operator()(const server::Trace<json::CancelOrder> &);

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  // connection
  core::web::Client connection_;
  // buffers
  core::Buffer decode_buffer_;
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile  //
        listen_key,
        listen_key_ack,                //
        account, account_ack,          //
        open_orders, open_orders_ack,  //
        new_order, new_order_ack,      //
        cancel_order, cancel_order_ack;
  } profile_;
  struct {
    core::metrics::Latency ping;
  } latency_;
  // security
  Security &security_;
  // cache
  Shared &shared_;
  std::string listen_key_;
  // state
  bool ready_ = false;
  std::chrono::nanoseconds listen_key_refresh_ = {};
  ConnectionStatus status_ = {};
  server::Download<OrderEntryState> download_;
};

}  // namespace binance
}  // namespace roq
