/* Copyright (c) 2017-2026, Hans Erik Thrane */

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

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/server/cache/cancel_order_request.hpp"

#include "roq/binance/gateway/order_entry.hpp"

#include "roq/binance/gateway/account.hpp"
#include "roq/binance/gateway/request.hpp"
#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/json/balances_ack.hpp"
#include "roq/binance/json/cancel_all_open_orders_ack.hpp"
#include "roq/binance/json/cancel_order_ack.hpp"
#include "roq/binance/json/listen_key_ack.hpp"
#include "roq/binance/json/new_order_ack.hpp"
#include "roq/binance/json/open_orders_ack.hpp"
#include "roq/binance/json/trades_ack.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct OrderEntryPortfolio final : public OrderEntry, public web::rest::Client::Handler {
  OrderEntryPortfolio(
      OrderEntry::Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &, bool master = true, std::string_view const &interface = {});

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

 protected:
  bool ready() const { return connection_status_ == ConnectionStatus::READY; }

  bool downloading() const { return download_account_ || download_orders_ || download_trades_; }

  // web::rest::Client::Handler
  void operator()(Trace<web::rest::Client::Connected> const &) override;
  void operator()(Trace<web::rest::Client::Disconnected> const &) override;
  void operator()(Trace<web::rest::Client::Header> const &) override;
  void operator()(Trace<web::rest::Client::Latency> const &) override;

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  enum class State {
    UNDEFINED = 0,
    LISTEN_KEY,
    DONE,
  };

  uint32_t download(State state);

  void get_listen_key();
  void get_listen_key_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::ListenKeyAck> const &);

  void get_account();
  void get_account_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::BalancesAck> const &);

  void get_open_orders();
  void get_open_orders_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::OpenOrdersAck> const &);

  void get_trades();
  void get_trades_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::TradesAck> const &);

  void refresh_listen_key(std::chrono::nanoseconds now);

  void new_order(Event<CreateOrder> const &, server::oms::Order const &, server::oms::RefData const &, std::string_view const &request_id);
  void new_order_ack(Trace<web::rest::Response> const &, uint8_t user_id, uint64_t order_id, uint32_t version);
  void operator()(Trace<json::NewOrderAck> const &, uint8_t user_id, uint64_t order_id, uint32_t version);

  void cancel_order(
      Event<CancelOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  void cancel_order_ack(Trace<web::rest::Response> const &, uint8_t user_id, uint64_t order_id, uint32_t version);
  void operator()(Trace<json::CancelOrderAck> const &, uint8_t user_id, uint64_t order_id, uint32_t version);

  void cancel_all_open_orders(Event<CancelAllOrders> const &, std::string_view const &request_id);
  void cancel_all_open_orders_ack(Trace<web::rest::Response> const &, std::string_view const &request_id);
  void operator()(Trace<json::CancelAllOpenOrdersAck> const &);

  // helpers

  void process_response(web::rest::Response const &, auto error_handler, auto success_handler);

  template <typename... Args>
  void operator()(Trace<server::oms::Response> const &, uint8_t user_id, uint64_t order_id, Args &&...);

  void operator()(Trace<server::oms::OrderUpdate> const &, std::string_view const &client_order_id);

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
  std::string encode_buffer_;
  bool download_trades_is_first_ = true;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
