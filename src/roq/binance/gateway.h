/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "roq/server.h"

#include "roq/core/io/context.h"

#include "roq/binance/config.h"
#include "roq/binance/drop_copy.h"
#include "roq/binance/market_data.h"
#include "roq/binance/order_entry.h"
#include "roq/binance/security.h"
#include "roq/binance/shared.h"

namespace roq {
namespace binance {

class Gateway final : public server::Handler,
                      public OrderEntry::Handler,
                      public DropCopy::Handler,
                      public MarketData::Handler {
 public:
  Gateway(server::Dispatcher &, const Config &);

 protected:
  void operator()(const Event<Start> &) override;
  void operator()(const Event<Stop> &) override;
  void operator()(const Event<Timer> &) override;
  void operator()(const Event<Connected> &) override;
  void operator()(const Event<Disconnected> &) override;

  uint16_t operator()(const Event<CreateOrder> &, const std::string_view &request_id) override;
  uint16_t operator()(
      const Event<ModifyOrder> &,
      const server::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id) override;
  uint16_t operator()(
      const Event<CancelOrder> &,
      const server::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id) override;

  uint16_t operator()(const Event<CancelAllOrders> &) override;

  void operator()(metrics::Writer &) override;

  // many

  void operator()(const server::Trace<StreamStatus> &) override;
  void operator()(const server::Trace<ExternalLatency> &) override;
  void operator()(const server::Trace<ReferenceData> &, bool is_last) override;
  void operator()(const server::Trace<MarketStatus> &, bool is_last) override;
  void operator()(const server::Trace<TopOfBook> &, bool is_last) override;
  void operator()(const server::Trace<MarketByPriceUpdate> &, bool is_last) override;
  void operator()(const server::Trace<TradeSummary> &, bool is_last) override;
  void operator()(const server::Trace<StatisticsUpdate> &, bool is_last) override;
  void operator()(const server::Trace<FundsUpdate> &, bool is_last) override;

  void operator()(const OrderEntry::ListenKeyUpdate &) override;
  void operator()(OrderEntry::SymbolsUpdate &) override;

  // utilities

  OrderEntry &get_order_entry(const std::string_view &account);

 private:
  server::Dispatcher &dispatcher_;
  // config
  const std::string master_account_;
  // security
  absl::flat_hash_map<std::string, std::unique_ptr<Security>> security_;
  // io
  core::io::Context context_;
  // shared
  Shared shared_;
  // seed
  uint16_t stream_id_ = {};
  // streams
  absl::flat_hash_map<std::string, std::unique_ptr<OrderEntry>> order_entry_;
  absl::flat_hash_map<std::string, std::unique_ptr<DropCopy>> drop_copy_;
  std::vector<std::unique_ptr<MarketData>> market_data_;
};

}  // namespace binance
}  // namespace roq
