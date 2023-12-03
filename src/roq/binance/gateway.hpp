/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "roq/server.hpp"

#include "roq/io/context.hpp"

#include "roq/binance/account.hpp"
#include "roq/binance/config.hpp"
#include "roq/binance/drop_copy.hpp"
#include "roq/binance/market_data.hpp"
#include "roq/binance/order_entry.hpp"
#include "roq/binance/request.hpp"
#include "roq/binance/rest.hpp"
#include "roq/binance/settings.hpp"
#include "roq/binance/shared.hpp"

namespace roq {
namespace binance {

struct Gateway final : public server::Handler,
                       public Rest::Handler,
                       public MarketData::Handler,
                       public OrderEntry::Handler,
                       public DropCopy::Handler {
  Gateway(server::Dispatcher &, Settings const &, Config const &, io::Context &);

 protected:
  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;
  void operator()(Event<Connected> const &) override;
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

  void operator()(metrics::Writer &) override;

  // many

  void operator()(Trace<StreamStatus> const &) override;
  void operator()(Trace<ExternalLatency> const &) override;
  void operator()(Trace<ReferenceData> const &, bool is_last) override;
  void operator()(Trace<MarketStatus> const &, bool is_last) override;
  void operator()(Trace<TopOfBook> const &, bool is_last) override;
  void operator()(Trace<MarketByPriceUpdate> const &, bool is_last) override;
  void operator()(Trace<TradeSummary> const &, bool is_last) override;
  void operator()(Trace<StatisticsUpdate> const &, bool is_last) override;
  void operator()(
      Trace<TradeUpdate> const &, bool is_last, uint8_t user_id, std::string_view const &request_id) override;
  void operator()(Trace<FundsUpdate> const &, bool is_last) override;

  void operator()(Rest::SymbolsUpdate &) override;

  void ensure_symbol_slices(size_t size);

  void operator()(OrderEntry::ListenKeyUpdate const &) override;

  // utilities

  template <typename... Args>
  void dispatch(Args &&...);

  OrderEntry &get_order_entry(std::string_view const &account);

 private:
  server::Dispatcher &dispatcher_;
  // accounts
  absl::flat_hash_map<std::string, std::unique_ptr<Account>> const accounts_;
  // io
  io::Context &context_;
  // shared
  Shared shared_;
  absl::flat_hash_map<std::string, Request> request_;
  // seed
  uint16_t stream_id_ = {};
  // EXPERIMENTAL
  struct OrderEntryRR final {
    OrderEntryRR(std::vector<std::unique_ptr<OrderEntry>> &&);

    template <typename... Args>
    void operator()(Args &&...);

    OrderEntry &get_next();

   private:
    std::vector<std::unique_ptr<OrderEntry>> order_entry_;
    size_t index_ = {};
  };
  // streams
  Rest rest_;
  absl::flat_hash_map<std::string, OrderEntryRR> order_entry_;
  absl::flat_hash_map<std::string, std::unique_ptr<DropCopy>> drop_copy_;
  std::vector<std::unique_ptr<MarketData>> market_data_1_, market_data_2_;
  // cache
  std::vector<MBPUpdate> bids_, asks_;
};

}  // namespace binance
}  // namespace roq
