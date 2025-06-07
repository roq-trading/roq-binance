/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "roq/utils/container.hpp"

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/gauge.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/core/download.hpp"

#include "roq/server.hpp"

#include "roq/server/cache/cancel_order_request.hpp"

#include "roq/binance/order_entry.hpp"

#include "roq/binance/account.hpp"
#include "roq/binance/order_entry_state.hpp"
#include "roq/binance/request.hpp"
#include "roq/binance/shared.hpp"

#include "roq/binance/json/account.hpp"
#include "roq/binance/json/cancel_all_open_orders.hpp"
#include "roq/binance/json/cancel_order.hpp"
#include "roq/binance/json/cancel_replace_order.hpp"
#include "roq/binance/json/cancel_replace_order_error.hpp"
#include "roq/binance/json/listen_key.hpp"
#include "roq/binance/json/new_order.hpp"
#include "roq/binance/json/open_orders.hpp"
#include "roq/binance/json/trades.hpp"

namespace roq {
namespace binance {

struct OrderEntryREST final : public OrderEntry, public web::rest::Client::Handler {
  OrderEntryREST(
      OrderEntry::Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &, bool master = true, std::string_view const &interface = {});

  bool ready() const override { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(metrics::Writer &) const override;

  void operator()(Event<Disconnected> const &) override;

  uint16_t operator()(Event<CreateOrder> const &, server::oms::Order const &, std::string_view const &request_id) override;
  uint16_t operator()(
      Event<ModifyOrder> const &, server::oms::Order const &, std::string_view const &request_id, std::string_view const &previous_request_id) override;
  uint16_t operator()(
      Event<CancelOrder> const &, server::oms::Order const &, std::string_view const &request_id, std::string_view const &previous_request_id) override;
  uint16_t operator()(Event<CancelAllOrders> const &, std::string_view const &request_id) override;

 protected:
  bool downloading() const {
    return download_account_ || download_orders_ || download_trades_ || download_account_cross_ || download_orders_cross_ || download_trades_cross_;
  }

  // web::rest::Client::Handler
  void operator()(Trace<web::rest::Client::Connected> const &) override;
  void operator()(Trace<web::rest::Client::Disconnected> const &) override;
  void operator()(Trace<web::rest::Client::Header> const &) override;
  void operator()(Trace<web::rest::Client::Latency> const &) override;

  void operator()(ConnectionStatus);

  uint32_t download(OrderEntryState state);

  void get_listen_key(MarginMode);
  void get_listen_key_ack(Trace<web::rest::Response> const &, MarginMode);
  void operator()(Trace<json::ListenKey> const &, MarginMode);

  void get_account(MarginMode);
  void get_account_ack(Trace<web::rest::Response> const &, MarginMode);
  void operator()(Trace<json::Account> const &, MarginMode);

  void get_open_orders(MarginMode);
  void get_open_orders_ack(Trace<web::rest::Response> const &, MarginMode);
  void operator()(Trace<json::OpenOrders> const &, MarginMode);

  void get_trades(MarginMode);
  void get_trades_ack(Trace<web::rest::Response> const &, MarginMode);
  void operator()(Trace<json::Trades> const &, MarginMode);

  void refresh_listen_key(std::chrono::nanoseconds now);

  void new_order(Event<CreateOrder> const &, server::oms::Order const &order, std::string_view const &request_id);
  void new_order_ack(Trace<web::rest::Response> const &, uint8_t user_id, uint64_t order_id, uint32_t version);
  void operator()(Trace<json::NewOrder> const &, uint8_t user_id, uint64_t order_id, uint32_t version);

  void cancel_replace_order(
      server::cache::CancelOrderRequest const &, Event<CreateOrder> const &, server::oms::Order const &, std::string_view const &_request_id);
  void cancel_replace_order_ack(
      Trace<web::rest::Response> const &,
      uint8_t user_id,
      uint64_t cancel_order_id,
      uint32_t cancel_version,
      uint64_t create_order_id,
      uint32_t create_version);
  void operator()(
      Trace<json::CancelReplaceOrder> const &,
      uint8_t user_id,
      uint64_t cancel_order_id,
      uint32_t cancel_version,
      uint64_t create_order_id,
      uint32_t create_version);
  void operator()(
      Trace<json::CancelReplaceOrderError> const &,
      uint8_t user_id,
      uint64_t cancel_order_id,
      uint32_t cancel_version,
      uint64_t create_order_id,
      uint32_t create_version);

  void cancel_order(Event<CancelOrder> const &, server::oms::Order const &, std::string_view const &request_id, std::string_view const &previous_request_id);
  void cancel_order_ack(Trace<web::rest::Response> const &, uint8_t user_id, uint64_t order_id, uint32_t version);
  void operator()(Trace<json::CancelOrder> const &, uint8_t user_id, uint64_t order_id, uint32_t version);

  void cancel_all_open_orders(Event<CancelAllOrders> const &, std::string_view const &request_id);
  void cancel_all_open_orders_ack(Trace<web::rest::Response> const &, std::string_view const &request_id);
  void operator()(Trace<json::CancelAllOpenOrders> const &);

  template <typename SuccessHandler, typename ErrorHandler>
  void process_response(web::rest::Response const &, SuccessHandler, ErrorHandler);

  template <typename... Args>
  void operator()(Trace<server::oms::Response> const &, uint8_t user_id, uint64_t order_id, Args &&...);

  void operator()(Trace<server::oms::OrderUpdate> const &, std::string_view const &client_order_id);

  template <typename Parse, typename Callback>
  void dispatch_error_2(web::rest::Response const &, web::http::Category, web::http::Status, Parse, Callback);  // XXX

  void test(web::http::Status);  // XXX
  void waf_limit_violation();

 private:
  OrderEntry::Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  bool const master_;
  // connection
  std::unique_ptr<web::rest::Client> connection_;
  // buffers
  std::vector<std::byte> decode_buffer_;
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile listen_key, listen_key_ack,  //
        account, account_ack,                            //
        open_orders, open_orders_ack,                    //
        trades, trades_ack,                              //
        new_order, new_order_ack,                        //
        cancel_replace_order, cancel_replace_order_ack,  //
        cancel_order, cancel_order_ack,                  //
        cancel_all_open_orders, cancel_all_open_orders_ack;
  } profile_;
  struct {
    utils::metrics::Latency ping;
  } latency_;
  struct {
    utils::metrics::Gauge requests_1m;
  } rate_limiter_;
  // authentication
  Account &account_;
  // shared
  Shared &shared_;
  Request &request_;
  std::string listen_key_;
  std::string listen_key_cross_;
  // state
  bool ready_ = false;
  std::chrono::nanoseconds listen_key_refresh_ = {};
  std::chrono::nanoseconds listen_key_refresh_cross_ = {};
  ConnectionStatus status_ = {};
  core::Download<OrderEntryState> download_;
  // experimental
  utils::unordered_set<std::string> open_orders_symbols_;
  bool download_account_ = false;
  bool download_orders_ = false;
  bool download_trades_ = false;
  bool download_account_cross_ = false;
  bool download_orders_cross_ = false;
  bool download_trades_cross_ = false;
  std::vector<char> encode_buffer_;
  bool download_trades_is_first_ = true;
  bool download_trades_cross_is_first_ = true;
};

}  // namespace binance
}  // namespace roq
