/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "roq/metrics.h"

#include "roq/server.h"
#include "roq/download.h"

#include "roq/core/hash/map.h"

#include "roq/core/ssl/ssl.h"

#include "roq/core/event/base.h"
#include "roq/core/event/dns_base.h"

#include "roq/binance/config.h"
#include "roq/binance/random.h"

#include "roq/binance/market_stream.h"

#include "roq/binance/rest.h"
#include "roq/binance/rest_state.h"

#include "roq/binance/json/account.h"
#include "roq/binance/json/exchange_info.h"
#include "roq/binance/json/listen_key.h"

// json (inbound)

namespace roq {
namespace binance {

class Gateway final
    : public server::Handler,
      public Rest::Handler,
      public MarketStream::Handler {
 public:
  Gateway(
      server::Dispatcher& dispatcher,
      const Config& config);

 protected:
  // server::Handler

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

  // MarketStream::Handler

  void operator()(const json::AggTrade&) override;
  void operator()(const json::Trade&) override;
  void operator()(const json::MiniTicker&) override;
  void operator()(const json::BookTicker&) override;
  void operator()(
      const std::string_view& symbol,
      const json::Depth& depth) override;
  void operator()(
      const std::string_view& symbol,
      const json::DepthUpdate& depth_update) override;

  // Rest::Handler

  void operator()(const Rest&) override;

 private:
  void operator()(const json::ExchangeInfo&);
  void operator()(const json::ListenKey&);
  void operator()(const json::Account&);

  void update_market_data(GatewayStatus gateway_status);
  void update_order_manager(GatewayStatus gateway_status);

  using RestDownload = server::Download<RestState>;

  uint32_t download(RestDownload::State state);

  void download_exchange_info();
  void subscribe_market_streams();
  void download_listen_key();
  void subscribe_user_stream();
  void download_account();

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
  struct {
    Rest connection;
    RestDownload download;
  } _rest;
  uint32_t _market_stream_id = 0;
  std::list<std::unique_ptr<MarketStream> > _market_streams;
  // reference data
  std::vector<std::string> _symbols;
  // market data
  GatewayStatus _market_data_status = GatewayStatus::DISCONNECTED;
  core::page_aligned_vector<MBPUpdate> _bid, _ask;
  // order manager
  GatewayStatus _order_manager_status = GatewayStatus::DISCONNECTED;
};

}  // namespace binance
}  // namespace roq
