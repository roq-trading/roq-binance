/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "roq/cache/cancel_order.hpp"

#include "roq/core/buffer.hpp"
#include "roq/core/download.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/server.hpp"

#include "roq/binance/authenticator.hpp"
#include "roq/binance/drop_copy_state.hpp"
#include "roq/binance/request.hpp"
#include "roq/binance/shared.hpp"

#include "roq/binance/json/ws_api_parser.hpp"

namespace roq {
namespace binance {

struct OrderEntryWS final : public web::socket::Client::Handler, public json::WSAPIParser::Handler {
  struct HoldCancelOrder final {
    cache::CancelOrder cancel_order;
    RequestId request_id;
    RequestId previous_request_id;
    Symbol symbol;
  };

  struct ListenKeyUpdate final {
    std::string_view account;
    std::string_view listen_key;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<oms::TradeUpdate> const &, uint16_t stream_id, bool is_last, uint8_t user_id) = 0;
    virtual void operator()(Trace<FundsUpdate> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(ListenKeyUpdate const &) = 0;
  };

  OrderEntryWS(Handler &, io::Context &, uint16_t stream_id, Authenticator &, Shared &, Request &);

  OrderEntryWS(OrderEntryWS &&) = delete;
  OrderEntryWS(OrderEntryWS const &) = delete;

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
  void user_data_stream_start();
  void user_data_stream_ping(std::chrono::nanoseconds now);

  void account_status();
  void open_orders_status();
  void open_orders_cancel_all(Event<CancelAllOrders> const &, std::string_view const &request_id);
  void order_place(Event<CreateOrder> const &, oms::Order const &, std::string_view const &request_id);
  void order_cancel(
      Event<CancelOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  void order_cancel_replace(
      HoldCancelOrder const &, Event<CreateOrder> const &, oms::Order const &, std::string_view const &_request_id);

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
  void operator()(Trace<json::CancelAllOpenOrders> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::NewOrder> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::CancelOrder> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::CancelReplaceOrder> const &, json::WSAPIRequest const &, int32_t status) override;
  void operator()(Trace<json::CancelReplaceOrderError> const &, json::WSAPIRequest const &, int32_t status) override;

  template <typename... Args>
  void operator()(Trace<oms::Response> const &, uint8_t user_id, uint32_t order_id, Args &&...args);

  template <typename... Args>
  void operator()(Trace<oms::OrderUpdate> const &, std::string_view const &client_order_id, Args &&...);

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  // web socket
  std::unique_ptr<web::socket::Client> connection_;
  // buffers
  core::Buffer decode_buffer_;
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile parse, error, user_data_stream_start, user_data_stream_start_ack, user_data_stream_ping,
        user_data_stream_ping_ack, account_status, account_status_ack, open_orders_status, open_orders_status_ack,
        open_orders_cancel_all, open_orders_cancel_all_ack, order_place, order_place_ack, order_cancel,
        order_cancel_ack, order_cancel_replace, order_cancel_replace_ack, order_cancel_replace_error;
  } profile_;
  struct {
    core::metrics::Latency ping, heartbeat;
  } latency_;
  // authentication
  Authenticator &authenticator_;
  // shared
  Shared &shared_;
  Request &request_;
  // experimental
  uint32_t request_id_;
  std::string listen_key_;
  std::chrono::nanoseconds listen_key_refresh_ = {};
  bool download_account_ = false;
  bool download_orders_ = false;
  absl::flat_hash_set<Symbol> open_orders_symbols_;
  std::vector<char> request_encode_buffer_;
  std::vector<char> encode_buffer_;
  // state
  bool ready_ = false;
  ConnectionStatus status_ = {};
  // batch
  std::vector<std::unique_ptr<HoldCancelOrder>> hold_cancel_order_;
};

}  // namespace binance
}  // namespace roq
