/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "roq/metrics.h"

#include "roq/server.h"

#include "roq/core/hash/map.h"

#include "roq/core/ssl/ssl.h"

#include "roq/core/event/base.h"
#include "roq/core/event/dns_base.h"

#include "roq/binance/config.h"
#include "roq/binance/random.h"

#include "roq/binance/rest.h"
#include "roq/binance/web_socket.h"

// json (inbound)

namespace roq {
namespace binance {

class WebSocket;

class Gateway final : public server::Handler {
 public:
  Gateway(
      server::Dispatcher& dispatcher,
      const Config& config);

  void operator()(const StartEvent&) override;
  void operator()(const StopEvent&) override;
  void operator()(const TimerEvent&) override;
  void operator()(const ConnectionStatusEvent&) override;

  void operator()(
      const CreateOrderEvent& event,
      const std::string_view& request_id,
      uint32_t gateway_order_id) override;
  void operator()(
      const ModifyOrderEvent& event,
      const std::string_view& request_id,
      const server::OMS_Order& order) override;
  void operator()(
      const CancelOrderEvent& event,
      const std::string_view& request_id,
      const server::OMS_Order& order) override;

  void operator()(Metrics& metrics) override;

  // ws
  void operator()(const WebSocket&);

  // rest
  void operator()(const Rest&);

 private:
  void update_market_data(GatewayStatus gateway_status);
  void update_order_manager(GatewayStatus gateway_status);

  void begin_download();
  void check_download();

  void download_accounts();

  void subscribe_instrument();
  void subscribe_order_book_l2();

 private:
  template <typename T>
  void enqueue(
      const T& event,
      bool is_last);

  template <typename T>
  void enqueue(
      uint8_t user_id,
      const T& event,
      bool is_last);

 private:
  server::Dispatcher& _dispatcher;
  // config
  const std::string _account;
  // authentication
  Random _random;
  // async
  core::event::Base _base;
  core::event::DNSBase _dns_base;
  // crypto
  core::ssl::Context _ssl_context;
  // connections
  WebSocket _web_socket;
  Rest _rest;
  // download
  enum class Download {
    NONE,
    PRODUCTS,
    ACCOUNTS,
    ORDER_BOOKS,
    READY,
  } _download = Download::NONE;
  // reference data
  std::vector<std::string> _symbols;
  struct {
    bool instrument = false;
    bool order_book_l2 = false;
  } _snapshot;
  // market data
  GatewayStatus _market_data_status = GatewayStatus::DISCONNECTED;
  core::page_aligned_vector<MBPUpdate> _bid, _ask;
  core::page_aligned_vector<Trade> _trade;
  std::unordered_map<uint64_t, double> _price_lookup;
  // order manager
  GatewayStatus _order_manager_status = GatewayStatus::DISCONNECTED;
};

}  // namespace binance
}  // namespace roq
