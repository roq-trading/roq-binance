/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

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
  void get_listen_key();

  void get_account();
  void get_open_orders();

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

  void operator()(Trace<json::ListenKey> const &) override;

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
    core::metrics::Profile parse, outbound_account_position, balance_update, execution_report, list_status;
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
  bool download_account_ = false;
  bool download_orders_ = false;
  // state
  bool ready_ = false;
  ConnectionStatus status_ = {};
};

}  // namespace binance
}  // namespace roq
