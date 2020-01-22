/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "roq/metrics.h"

#include "roq/server.h"

#include "roq/core/hash/map.h"

#include "roq/core/ssl/ssl.h"

#include "roq/core/event/base.h"
#include "roq/core/event/dns_base.h"

#include "roq/binance/config.h"

#include "roq/binance/order_mapping.h"
#include "roq/binance/product.h"
#include "roq/binance/rest.h"
#include "roq/binance/websocket.h"

// json (inbound)
#include "roq/binance/json/accounts.h"
#include "roq/binance/json/activate.h"
#include "roq/binance/json/change.h"
#include "roq/binance/json/done.h"
#include "roq/binance/json/error.h"
#include "roq/binance/json/heartbeat.h"
#include "roq/binance/json/l2update.h"
#include "roq/binance/json/last_match.h"
#include "roq/binance/json/match.h"
#include "roq/binance/json/open.h"
#include "roq/binance/json/products.h"
#include "roq/binance/json/received.h"
#include "roq/binance/json/snapshot.h"
#include "roq/binance/json/status.h"
#include "roq/binance/json/subscriptions.h"
#include "roq/binance/json/ticker.h"

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
  void operator()(const CreateOrderEvent&) override;
  void operator()(const ModifyOrderEvent&) override;
  void operator()(const CancelOrderEvent&) override;

  void operator()(Metrics& metrics) override;

  // ws
  void operator()(const WebSocket&);
  void operator()(const json::Error& error);
  void operator()(const json::Heartbeat& heartbeat);
  void operator()(const json::Subscriptions& subscriptions);
  void operator()(const json::Status& status);
  void operator()(const json::Received& received);
  void operator()(const json::Open& open);
  void operator()(
      const json::Match& match,
      bool trade_summary,
      bool trade_update);
  void operator()(const json::Done& done);
  void operator()(const json::Change& change);
  void operator()(const json::Activate& activate);
  void operator()(const json::Ticker& ticker);
  void operator()(const json::Snapshot& snapshot);
  void operator()(const json::L2Update& l2update);
  void operator()(const json::LastMatch& last_match);

  // rest
  void operator()(const Rest&);
  void operator()(const json::Products&);
  void operator()(const json::Accounts&);
  // TODO(thraneh): error

 private:
  void update_market_data(GatewayStatus gateway_status);
  void update_order_manager(GatewayStatus gateway_status);

  void begin_download();
  void check_download();

  void download_products();
  void download_accounts();

  void subscribe();

  void operator()(const json::Product& product);
  void operator()(const json::Account& account);

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

  bool validate(const CreateOrderEvent& event);

  bool validate(
      const CancelOrderEvent& event,
      uint32_t gateway_order_id,
      const std::string_view& external_order_id);

 private:
  server::Dispatcher& _dispatcher;
  // config
  const std::string _account;
  // async
  core::event::Base _base;
  core::event::DNSBase _dns_base;
  // crypto
  core::ssl::Context _ssl_context;
  // connections
  WebSocket _websocket;
  Rest _rest;
  // download
  enum class Download {
    NONE,
    PRODUCTS,
    ACCOUNTS,
  } _download = Download::NONE;
  std::vector<std::string> _symbols;
  // reference data
  core::hash::map<std::string, Product> _product_cache;
  // market data
  GatewayStatus _market_data_status = GatewayStatus::DISCONNECTED;
  core::page_aligned_vector<MBPUpdate> _bid, _ask;
  // order manager
  GatewayStatus _order_manager_status = GatewayStatus::DISCONNECTED;
  std::unordered_map<uint64_t, OrderMapping> _order_mapping;
  core::hash::map<std::string, uint64_t> _order_lookup;

  decltype(_order_mapping)::iterator find_order_mapping(  // XXX move
      const std::string_view& order_id);
  decltype(_order_mapping)::iterator find_order_mapping(  // XXX move
      const std::string_view& order_id,
      const std::string_view& cl_ord_id);
};

}  // namespace binance
}  // namespace roq
