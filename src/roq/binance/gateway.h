/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "roq/metrics.h"

#include "roq/download.h"
#include "roq/server.h"

#include "roq/core/ssl/ssl.h"

#include "roq/core/event/base.h"
#include "roq/core/event/dns_base.h"

#include "roq/binance/config.h"
#include "roq/binance/random.h"

#include "roq/binance/market_stream.h"
#include "roq/binance/user_stream.h"

#include "roq/binance/rest.h"
#include "roq/binance/rest_state.h"

#include "roq/binance/json/account.h"
#include "roq/binance/json/exchange_info.h"
#include "roq/binance/json/listen_key.h"

#include "roq/binance/json/cancel_order.h"
#include "roq/binance/json/new_order.h"

// json (inbound)

namespace roq {
namespace binance {

class Gateway final : public server::Handler,
                      public Rest::Handler,
                      public MarketStream::Handler,
                      public UserStream::Handler {
 public:
  Gateway(server::Dispatcher &dispatcher, const Config &config);

 protected:
  // server::Handler

  void operator()(const Event<Start> &) override;
  void operator()(const Event<Stop> &) override;
  void operator()(const Event<Timer> &) override;
  void operator()(const Event<Connection> &) override;

  void operator()(
      const Event<CreateOrder> &event,
      const std::string_view &request_id,
      uint32_t gateway_order_id) override;
  void operator()(
      const Event<ModifyOrder> &event,
      const std::string_view &request_id,
      const server::OMS_Order &order) override;
  void operator()(
      const Event<CancelOrder> &event,
      const std::string_view &request_id,
      const server::OMS_Order &order) override;

  void operator()(metrics::Writer &writer) override;

  // MarketStream::Handler

  void operator()(const json::AggTrade &, const server::TraceInfo &) override;
  void operator()(const json::Trade &, const server::TraceInfo &) override;
  void operator()(const json::MiniTicker &, const server::TraceInfo &) override;
  void operator()(const json::BookTicker &, const server::TraceInfo &) override;
  void operator()(
      const std::string_view &symbol,
      const json::Depth &depth,
      const server::TraceInfo &) override;
  void operator()(
      const std::string_view &symbol,
      const json::DepthUpdate &depth_update,
      const server::TraceInfo &) override;

  // UserStream::Handler
  void operator()(
      const json::OutboundAccountInfo &, const server::TraceInfo &) override;
  void operator()(
      const json::OutboundAccountPosition &,
      const server::TraceInfo &) override;
  void operator()(
      const json::BalanceUpdate &, const server::TraceInfo &) override;
  void operator()(
      const json::ExecutionReport &, const server::TraceInfo &) override;

  // Rest::Handler

  void operator()(const Rest &) override;

  void operator()(const json::NewOrder &);
  void operator()(const json::CancelOrder &);

 private:
  void operator()(const json::ExchangeInfo &);
  void operator()(const json::ListenKey &);
  void operator()(const json::Account &);

  void update_market_data(GatewayStatus gateway_status);
  void update_order_manager(GatewayStatus gateway_status);

  using RestDownload = server::Download<RestState>;

  uint32_t download(RestDownload::State state);

  void download_exchange_info();
  void subscribe_market_streams();
  void download_listen_key();
  void subscribe_user_stream();
  void download_account();

  void refresh_listen_key();

  /*
  template <typename T>
  void enqueue(
      const T& event,
      const server::Trace& trace,
      bool is_last);

  template <typename T>
  void enqueue(
      uint8_t user_id,
      const T& event,
      const server::Trace& trace,
      bool is_last);
      */

 private:
  server::Dispatcher &dispatcher_;
  // config
  const std::string account_;
  // authentication
  Random random_;
  // async
  core::event::Base base_;
  core::event::DNSBase dns_base_;
  // crypto
  core::ssl::Context ssl_context_;
  // connections
  struct {
    Rest connection;
    RestDownload download;
  } rest_;
  // ... market streams
  uint32_t market_stream_id_ = 0;
  std::list<std::unique_ptr<MarketStream> > market_streams_;
  // ... user streams
  std::string listen_key_;
  std::chrono::nanoseconds listen_key_refresh_ = {};
  std::unique_ptr<UserStream> user_stream_;
  // reference data
  absl::flat_hash_set<std::string> symbols_;
  // market data
  GatewayStatus market_data_status_ = GatewayStatus::DISCONNECTED;
  core::page_aligned_vector<MBPUpdate> bid_, ask_;
  // order manager
  GatewayStatus order_manager_status_ = GatewayStatus::DISCONNECTED;
};

}  // namespace binance
}  // namespace roq
