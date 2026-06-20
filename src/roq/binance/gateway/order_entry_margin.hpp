/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <memory>
#include <string>

#include "roq/utils/container.hpp"

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/gauge.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/core/download.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/binance/gateway/order_entry.hpp"

#include "roq/binance/gateway/account.hpp"
#include "roq/binance/gateway/request.hpp"
#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/protocol/json/account_ack.hpp"
#include "roq/binance/protocol/json/cancel_all_open_orders_ack.hpp"
#include "roq/binance/protocol/json/cancel_order_ack.hpp"
#include "roq/binance/protocol/json/cancel_replace_order_ack.hpp"
#include "roq/binance/protocol/json/cancel_replace_order_error.hpp"
#include "roq/binance/protocol/json/cross_margin_account.hpp"
#include "roq/binance/protocol/json/listen_key_ack_margin.hpp"
#include "roq/binance/protocol/json/new_order_ack.hpp"
#include "roq/binance/protocol/json/open_orders_ack.hpp"
#include "roq/binance/protocol/json/trades_ack.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct OrderEntryMargin final : public OrderEntry, public web::rest::Client::Handler {
  OrderEntryMargin(OrderEntry::Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &);

 protected:
  // OrderEntry

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

  // helpers

  bool ready() const { return connection_status_ == ConnectionStatus::READY; }

  bool downloading() const {
    return download_account_ || download_orders_ || download_trades_ || download_listen_key_cross_ || download_account_cross_ || download_orders_cross_ ||
           download_trades_cross_ || download_account_cross_on_timer_;
  }

  // web::rest::Client::Handler

  void operator()(Trace<web::rest::Client::Connected> const &) override;
  void operator()(Trace<web::rest::Client::Disconnected> const &) override;
  void operator()(Trace<web::rest::Client::Header> const &) override;
  void operator()(Trace<web::rest::Client::Latency> const &) override;

  // helpers

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  enum class State {
    UNDEFINED = 0,
    LISTEN_KEY,
    DONE,
  };

  uint32_t download(State state);

  // listen-key

  void get_listen_key(MarginMode);
  void get_listen_key_ack(Trace<web::rest::Response> const &, MarginMode);
  void operator()(Trace<protocol::json::ListenKeyAckMargin> const &, MarginMode);

  // account

  void get_account(MarginMode);
  void get_account_ack(Trace<web::rest::Response> const &, MarginMode);
  void operator()(Trace<protocol::json::AccountAck> const &, MarginMode);
  void operator()(Trace<protocol::json::CrossMarginAccount> const &, MarginMode);

  // open-orders

  void get_open_orders(MarginMode);
  void get_open_orders_ack(Trace<web::rest::Response> const &, MarginMode);
  void operator()(Trace<protocol::json::OpenOrdersAck> const &, MarginMode);

  // trades

  void get_trades(MarginMode);
  void get_trades_ack(Trace<web::rest::Response> const &, MarginMode);
  void operator()(Trace<protocol::json::TradesAck> const &, MarginMode);

  // account-cross

  void get_account_cross_on_timer();
  void get_account_cross_on_timer_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<protocol::json::CrossMarginAccount> const &);

  // listen-key

  void refresh_listen_key(std::chrono::nanoseconds now);

  // new-order

  void new_order(Event<CreateOrder> const &, server::oms::Order const &order, server::oms::RefData const &, std::string_view const &request_id);
  void new_order_ack(Trace<web::rest::Response> const &, uint8_t user_id, uint64_t order_id, uint32_t version);
  void operator()(Trace<protocol::json::NewOrderAck> const &, uint8_t user_id, uint64_t order_id, uint32_t version);

  // cancel-order

  void cancel_order(
      Event<CancelOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  void cancel_order_ack(Trace<web::rest::Response> const &, uint8_t user_id, uint64_t order_id, uint32_t version);
  void operator()(Trace<protocol::json::CancelOrderAck> const &, uint8_t user_id, uint64_t order_id, uint32_t version);

  // cancel-all-open-orders

  void cancel_all_open_orders(Event<CancelAllOrders> const &, std::string_view const &request_id);
  void cancel_all_open_orders_ack(Trace<web::rest::Response> const &, std::string_view const &request_id);
  void operator()(Trace<protocol::json::CancelAllOpenOrdersAck> const &);

  // helpers

  void process_response(web::rest::Response const &, auto error_handler, auto success_handler);

  template <typename Parse, typename Callback>
  void dispatch_error_2(web::rest::Response const &, web::http::Category, web::http::Status, Parse, Callback);  // XXX

  void test(web::http::Status);  // XXX
  void waf_limit_violation();

 private:
  OrderEntry::Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // connection
  std::unique_ptr<web::rest::Client> connection_;
  // buffers
  core::json::BufferStack decode_buffer_;
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
  ConnectionStatus connection_status_ = {};
  core::Download<State> download_;
  // experimental
  utils::unordered_set<std::string> open_orders_symbols_;
  bool download_account_ = false;
  bool download_orders_ = false;
  bool download_trades_ = false;
  bool download_listen_key_cross_ = false;
  bool download_account_cross_ = false;
  bool download_orders_cross_ = false;
  bool download_trades_cross_ = false;
  bool download_account_cross_on_timer_ = false;
  std::string encode_buffer_;
  bool download_trades_is_first_ = true;
  bool download_trades_cross_is_first_ = true;
  //
  std::chrono::nanoseconds next_poll_borrowed_ = {};
  //
  bool has_listen_key_cross_ = false;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
