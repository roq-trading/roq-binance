/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "roq/server.hpp"

#include "roq/core/io/context.hpp"

#include "roq/binance/config.hpp"
#include "roq/binance/drop_copy.hpp"
#include "roq/binance/market_data.hpp"
#include "roq/binance/order_entry.hpp"
#include "roq/binance/request.hpp"
#include "roq/binance/rest.hpp"
#include "roq/binance/security.hpp"
#include "roq/binance/shared.hpp"

namespace roq {
namespace binance {

class Gateway final : public server::Handler,
                      public Rest::Handler,
                      public MarketData::Handler,
                      public OrderEntry::Handler,
                      public DropCopy::Handler {
 public:
  Gateway(server::Dispatcher &, const Config &);

 protected:
  void operator()(const Event<Start> &) override;
  void operator()(const Event<Stop> &) override;
  void operator()(const Event<Timer> &) override;
  void operator()(const Event<Connected> &) override;
  void operator()(const Event<Disconnected> &) override;

  uint16_t operator()(
      const Event<CreateOrder> &, const oms::Order &, const std::string_view &request_id) override;
  uint16_t operator()(
      const Event<ModifyOrder> &,
      const oms::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id) override;
  uint16_t operator()(
      const Event<CancelOrder> &,
      const oms::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id) override;

  uint16_t operator()(const Event<CancelAllOrders> &, const std::string_view &request_id) override;

  void operator()(metrics::Writer &) override;

  // many

  void operator()(const Trace<StreamStatus> &) override;
  void operator()(const Trace<ExternalLatency> &) override;
  void operator()(const Trace<ReferenceData> &, bool is_last) override;
  void operator()(const Trace<MarketStatus> &, bool is_last) override;
  void operator()(const Trace<TopOfBook> &, bool is_last) override;
  void operator()(const Trace<MarketByPriceUpdate> &, bool is_last, bool refresh) override;
  void operator()(const Trace<TradeSummary> &, bool is_last) override;
  void operator()(const Trace<StatisticsUpdate> &, bool is_last) override;
  void operator()(const Trace<TradeUpdate> &, bool is_last, uint8_t user_id) override;
  void operator()(const Trace<FundsUpdate> &, bool is_last) override;

  void operator()(Rest::SymbolsUpdate &) override;

  void ensure_symbol_slices(size_t size);

  void operator()(const OrderEntry::ListenKeyUpdate &) override;

  // utilities

  OrderEntry &get_order_entry(const std::string_view &account);

 private:
  server::Dispatcher &dispatcher_;
  // security
  absl::flat_hash_map<Account, std::unique_ptr<Security>> security_;
  // io
  core::io::Context context_;
  // shared
  Shared shared_;
  absl::flat_hash_map<Account, Request> request_;
  // seed
  uint16_t stream_id_ = {};
  // streams
  Rest rest_;
  absl::flat_hash_map<Account, std::unique_ptr<OrderEntry>> order_entry_;
  absl::flat_hash_map<Account, std::unique_ptr<DropCopy>> drop_copy_;
  std::vector<std::unique_ptr<MarketData>> market_data_;
};

}  // namespace binance
}  // namespace roq
