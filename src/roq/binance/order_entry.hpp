/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <string>
#include <string_view>

#include "roq/core/buffer.hpp"
#include "roq/core/download.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/core/io/context.hpp"

#include "roq/core/web/client.hpp"

#include "roq/server.hpp"

#include "roq/binance/order_entry_state.hpp"
#include "roq/binance/request.hpp"
#include "roq/binance/security.hpp"
#include "roq/binance/shared.hpp"

#include "roq/binance/json/account.hpp"
#include "roq/binance/json/cancel_all_open_orders.hpp"
#include "roq/binance/json/cancel_order.hpp"
#include "roq/binance/json/listen_key.hpp"
#include "roq/binance/json/new_order.hpp"
#include "roq/binance/json/open_orders.hpp"

namespace roq {
namespace binance {

class OrderEntry final : public core::web::Client::Handler {
 public:
  struct ListenKeyUpdate final {
    std::string_view account;
    std::string_view listen_key;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus const> const &) = 0;
    virtual void operator()(Trace<ExternalLatency const> const &) = 0;
    virtual void operator()(Trace<FundsUpdate const> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(ListenKeyUpdate const &) = 0;
  };

  OrderEntry(Handler &, core::io::Context &, uint16_t stream_id, Security &, Shared &, Request &);

  OrderEntry(OrderEntry &&) = delete;
  OrderEntry(OrderEntry const &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }
  bool downloading() const { return download_account_ || download_orders_; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &);

  uint16_t operator()(Event<CreateOrder> const &, oms::Order const &, std::string_view const &request_id);
  uint16_t operator()(
      Event<ModifyOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  uint16_t operator()(
      Event<CancelOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  uint16_t operator()(Event<CancelAllOrders> const &, std::string_view const &request_id);

 protected:
  void operator()(core::web::Client::Connected const &);
  void operator()(core::web::Client::Disconnected const &);
  void operator()(core::web::Client::Latency const &);

  void operator()(ConnectionStatus);

  uint32_t download(OrderEntryState state);

  void get_listen_key();
  void get_listen_key_ack(Trace<core::web::Response const> const &, uint32_t sequence);
  void operator()(Trace<json::ListenKey const> const &);

  void get_account();
  void get_account_ack(Trace<core::web::Response const> const &);
  void operator()(Trace<json::Account const> const &);

  void get_open_orders();
  void get_open_orders_ack(Trace<core::web::Response const> const &);
  void operator()(Trace<json::OpenOrders const> const &);

  void refresh_listen_key();

  void new_order(Event<CreateOrder> const &, oms::Order const &order, std::string_view const &request_id);
  void new_order_ack(Trace<core::web::Response const> const &, uint8_t user_id, uint32_t order_id, uint32_t version);
  void operator()(Trace<json::NewOrder const> const &, uint8_t user_id, uint32_t order_id, uint32_t version);

  void cancel_order(
      Event<CancelOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  void cancel_order_ack(Trace<core::web::Response const> const &, uint8_t user_id, uint32_t order_id, uint32_t version);
  void operator()(Trace<json::CancelOrder const> const &, uint8_t user_id, uint32_t order_id, uint32_t version);

  void cancel_all_open_orders(Event<CancelAllOrders> const &, std::string_view const &request_id);
  void cancel_all_open_orders_ack(Trace<core::web::Response const> const &);
  void operator()(Trace<json::CancelAllOpenOrders const> const &);

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
    core::metrics::Profile listen_key, listen_key_ack,  //
        account, account_ack,                           //
        open_orders, open_orders_ack,                   //
        new_order, new_order_ack,                       //
        cancel_order, cancel_order_ack,                 //
        cancel_all_open_orders, cancel_all_open_orders_ack;
  } profile_;
  struct {
    core::metrics::Latency ping;
  } latency_;
  // security
  Security &security_;
  // shared
  Shared &shared_;
  Request &request_;
  std::string listen_key_;
  // state
  bool ready_ = false;
  std::chrono::nanoseconds listen_key_refresh_ = {};
  ConnectionStatus status_ = {};
  core::Download<OrderEntryState> download_;
  // experimental
  absl::flat_hash_set<Symbol> open_orders_symbols_;
  bool download_account_ = false;
  bool download_orders_ = false;
};

}  // namespace binance
}  // namespace roq
