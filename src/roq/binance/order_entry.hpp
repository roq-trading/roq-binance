/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <memory>
#include <string>
#include <string_view>

#include "roq/cache/cancel_order.hpp"

#include "roq/core/buffer.hpp"
#include "roq/core/download.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/server.hpp"

#include "roq/binance/order_entry_state.hpp"
#include "roq/binance/request.hpp"
#include "roq/binance/security.hpp"
#include "roq/binance/shared.hpp"

#include "roq/binance/json/account.hpp"
#include "roq/binance/json/cancel_all_open_orders.hpp"
#include "roq/binance/json/cancel_order.hpp"
#include "roq/binance/json/cancel_replace_order.hpp"
#include "roq/binance/json/cancel_replace_order_error.hpp"
#include "roq/binance/json/listen_key.hpp"
#include "roq/binance/json/new_order.hpp"
#include "roq/binance/json/open_orders.hpp"

namespace roq {
namespace binance {

struct OrderEntry final : public web::rest::Client::Handler {
  struct HoldCancelOrder final {
    cache::CancelOrder cancel_order;
    RequestId request_id;
    RequestId previous_request_id;
    Symbol symbol;
  };

 public:
  struct ListenKeyUpdate final {
    std::string_view account;
    std::string_view listen_key;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<FundsUpdate> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(ListenKeyUpdate const &) = 0;
  };

  OrderEntry(Handler &, io::Context &, uint16_t stream_id, Security &, Shared &, Request &);

  OrderEntry(OrderEntry &&) = delete;
  OrderEntry(OrderEntry const &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }
  bool downloading() const { return download_account_ || download_orders_; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &);

  void operator()(Event<Disconnected> const &);

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
  void operator()(Trace<web::rest::Client::Connected> const &) override;
  void operator()(Trace<web::rest::Client::Disconnected> const &) override;
  void operator()(Trace<web::rest::Client::Latency> const &) override;
  void operator()(Trace<web::rest::Response> const &, uint64_t request_id, uint64_t opaque) override;

  void operator()(ConnectionStatus);

  uint32_t download(OrderEntryState state);

  void get_listen_key();
  void get_listen_key_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::ListenKey> const &);

  void get_account();
  void get_account_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::Account> const &);

  void get_open_orders();
  void get_open_orders_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::OpenOrders> const &);

  void refresh_listen_key(std::chrono::nanoseconds now);

  void new_order(Event<CreateOrder> const &, oms::Order const &order, std::string_view const &request_id);
  void new_order_ack(Trace<web::rest::Response> const &, uint8_t user_id, uint32_t order_id, uint32_t version);
  void new_order_ack_2(Trace<web::rest::Response> const &, uint64_t opaque);
  void operator()(Trace<json::NewOrder> const &, uint8_t user_id, uint32_t order_id, uint32_t version);

  void cancel_replace_order(
      HoldCancelOrder const &, Event<CreateOrder> const &, oms::Order const &, std::string_view const &_request_id);
  void cancel_replace_order_ack(
      Trace<web::rest::Response> const &,
      uint8_t user_id,
      uint32_t cancel_order_id,
      uint32_t cancel_version,
      uint32_t create_order_id,
      uint32_t create_version);
  void cancel_replace_order_ack_2(Trace<web::rest::Response> const &, uint64_t opaque);
  void operator()(
      Trace<json::CancelReplaceOrder> const &,
      uint8_t user_id,
      uint32_t cancel_order_id,
      uint32_t cancel_version,
      uint32_t create_order_id,
      uint32_t create_version);
  void operator()(
      Trace<json::CancelReplaceOrderError> const &,
      uint8_t user_id,
      uint32_t cancel_order_id,
      uint32_t cancel_version,
      uint32_t create_order_id,
      uint32_t create_version);

  void cancel_order(
      Event<CancelOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  void cancel_order_ack(Trace<web::rest::Response> const &, uint8_t user_id, uint32_t order_id, uint32_t version);
  void cancel_order_ack_2(Trace<web::rest::Response> const &, uint64_t opaque);
  void operator()(Trace<json::CancelOrder> const &, uint8_t user_id, uint32_t order_id, uint32_t version);

  void cancel_all_open_orders(Event<CancelAllOrders> const &, std::string_view const &request_id);
  void cancel_all_open_orders_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::CancelAllOpenOrders> const &);

  template <typename SuccessHandler, typename ErrorHandler>
  void process_response(web::rest::Response const &, SuccessHandler, ErrorHandler);

  template <typename... Args>
  void operator()(Trace<oms::Response> const &, uint8_t user_id, uint32_t order_id, Args &&...);

  template <typename... Args>
  void operator()(Trace<oms::OrderUpdate> const &, std::string_view const &client_order_id, Args &&...);

  template <typename Parse, typename Callback>
  void dispatch_error_2(web::http::Category, web::http::Status, Parse, Callback);  // XXX

  void test(web::http::Status);  // XXX
  void waf_limit_violation();

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  // connection
  std::unique_ptr<web::rest::Client> connection_;
  // buffers
  core::Buffer decode_buffer_;
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile listen_key, listen_key_ack,   //
        account, account_ack,                            //
        open_orders, open_orders_ack,                    //
        new_order, new_order_ack,                        //
        cancel_replace_order, cancel_replace_order_ack,  //
        cancel_order, cancel_order_ack,                  //
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
  std::vector<char> encode_buffer_;
  // batch
  std::vector<std::unique_ptr<HoldCancelOrder>> hold_cancel_order_;
};

}  // namespace binance
}  // namespace roq
