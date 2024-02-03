/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/download.hpp"

#include "roq/server.hpp"

#include "roq/server/cache/cancel_order_request.hpp"

#include "roq/binance/order_entry.hpp"

#include "roq/binance/account.hpp"
#include "roq/binance/request.hpp"
#include "roq/binance/shared.hpp"

#include "roq/binance/json/ws_api_parser.hpp"

namespace roq {
namespace binance {

struct OrderEntryWS final : public OrderEntry, public web::socket::Client::Handler, public json::WSAPIParser::Handler {
  OrderEntryWS(
      OrderEntry::Handler &,
      io::Context &,
      uint16_t stream_id,
      Account &,
      Shared &,
      Request &,
      bool master = true,
      std::string_view const &interface = {});

  bool ready() const override { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(metrics::Writer &) override;

  void operator()(Event<Disconnected> const &) override;

  uint16_t operator()(Event<CreateOrder> const &, oms::Order const &, std::string_view const &request_id) override;
  uint16_t operator()(
      Event<ModifyOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) override;
  uint16_t operator()(
      Event<CancelOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) override;
  uint16_t operator()(Event<CancelAllOrders> const &, std::string_view const &request_id) override;

 protected:
  bool downloading() const { return download_account_ || download_orders_ || download_trades_; }

  void user_data_stream_start();
  void user_data_stream_ping(std::chrono::nanoseconds now);

  void account_status();
  void open_orders_status();

  void my_trades();

  void open_orders_cancel_all(Event<CancelAllOrders> const &, std::string_view const &request_id);
  void order_place(Event<CreateOrder> const &, oms::Order const &, std::string_view const &request_id);
  void order_cancel(
      Event<CancelOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  void order_cancel_replace(
      server::cache::CancelOrderRequest const &,
      Event<CreateOrder> const &,
      oms::Order const &,
      std::string_view const &_request_id);

  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

 private:
  void operator()(ConnectionStatus);

  void parse(std::string_view const &message);

  void operator()(Trace<json::Error> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::ListenKey> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::Account> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::OpenOrders> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::Trades> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::CancelAllOpenOrders> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::NewOrder> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::CancelOrder> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::CancelReplaceOrder> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::CancelReplaceOrderError> const &, json::WSAPIRequest const &, int32_t status) override;

  void update_helper(
      Trace<json::CancelReplaceOrder> const &,
      json::WSAPIRequest const &,
      int32_t status,
      int32_t error_code,
      std::string_view const &error_msg);

  template <typename... Args>
  void operator()(Trace<oms::Response> const &, uint8_t user_id, uint64_t order_id, Args &&...args);

  void operator()(Trace<oms::OrderUpdate> const &, std::string_view const &client_order_id);

 private:
  OrderEntry::Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  bool master_;
  // web socket
  std::unique_ptr<web::socket::Client> connection_;
  // buffers
  std::vector<std::byte> decode_buffer_;
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile parse, error, user_data_stream_start, user_data_stream_start_ack, user_data_stream_ping,
        user_data_stream_ping_ack, account_status, account_status_ack, open_orders_status, open_orders_status_ack,
        my_trades, my_trades_ack, open_orders_cancel_all, open_orders_cancel_all_ack, order_place, order_place_ack,
        order_cancel, order_cancel_ack, order_cancel_replace, order_cancel_replace_ack, order_cancel_replace_error;
  } profile_;
  struct {
    utils::metrics::Latency ping, heartbeat;
  } latency_;
  // authentication
  Account &account_;
  // shared
  Shared &shared_;
  Request &request_;
  // experimental
  uint32_t request_id_;
  std::string listen_key_;
  std::chrono::nanoseconds listen_key_refresh_ = {};
  bool download_account_ = false;
  bool download_orders_ = false;
  bool download_trades_ = false;
  absl::flat_hash_set<Symbol> open_orders_symbols_;
  std::vector<char> request_encode_buffer_;
  std::vector<char> encode_buffer_;
  // state
  bool ready_ = false;
  ConnectionStatus status_ = {};
  bool download_trades_is_first_ = true;
};

}  // namespace binance
}  // namespace roq
