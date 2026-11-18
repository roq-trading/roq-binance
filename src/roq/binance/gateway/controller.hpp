/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include "roq/compat.hpp"

#include <memory>
#include <string>
#include <vector>

#include "roq/server.hpp"

#include "roq/utils/container.hpp"

#include "roq/io/context.hpp"

#include "roq/binance/gateway/account.hpp"
#include "roq/binance/gateway/config.hpp"
#include "roq/binance/gateway/request.hpp"
#include "roq/binance/gateway/settings.hpp"
#include "roq/binance/gateway/shared.hpp"

#include "roq/binance/gateway/drop_copy_margin.hpp"
#include "roq/binance/gateway/drop_copy_portfolio.hpp"
#include "roq/binance/gateway/market_data.hpp"
#include "roq/binance/gateway/order_entry_margin.hpp"
#include "roq/binance/gateway/order_entry_portfolio.hpp"
#include "roq/binance/gateway/rest.hpp"
#include "roq/binance/gateway/web_socket.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct Controller final : public Base<Controller>,
                          public server::Handler,
                          public Rest::Handler,
                          public MarketData::Handler,
                          public OrderEntryMargin::Handler,
                          public OrderEntryPortfolio::Handler,
                          public WebSocket::Handler,
                          public DropCopyMargin::Handler,
                          public DropCopyPortfolio::Handler {
  ROQ_PUBLIC static std::unique_ptr<server::Handler> create(server::Dispatcher &, Settings const &, Config const &, io::Context &);

  ROQ_PUBLIC static uint8_t parse_api(Settings const &);

  Controller(server::Dispatcher &, Settings const &, Config const &, io::Context &);

  Controller(Controller const &) = delete;

 protected:
  friend base_type;

  // server::Handler

  void operator()(Trace<Start> const &) override;
  void operator()(Trace<Stop> const &) override;
  void operator()(Trace<Timer> const &) override;

  void operator()(Event<Connected> const &) override;
  void operator()(Event<Disconnected> const &) override;

  void operator()(Trace<Subscribe> const &) override;
  void operator()(Trace<Control> const &) override;

  void operator()(Trace<server::oms::OrderActionBatchBegin> const &) override;
  void operator()(Trace<server::oms::OrderActionBatchCommit> const &) override;
  void operator()(Trace<server::oms::OrderActionBatchRollback> const &) override;

  uint16_t operator()(Trace<CreateOrder> const &, server::oms::Order const &, server::oms::RefData const &, std::string_view const &request_id) override;
  uint16_t operator()(
      Trace<ModifyOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) override;
  uint16_t operator()(
      Trace<CancelOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) override;

  uint16_t operator()(Trace<CancelAllOrders> const &, std::string_view const &request_id) override;

  uint16_t operator()(Trace<MassQuote> const &) override;

  uint16_t operator()(Trace<CancelQuotes> const &) override;

  void operator()(metrics::Writer &) const override;

  // OrderEntry::Handler

  void operator()(Trace<ListenKeyUpdate> const &) override;

  // Rest::Handler

  void operator()(Trace<SymbolsUpdate> const &) override;

  // helpers

  void ensure_symbol_slices(TraceInfo const &, size_t size);

  template <typename... Args>
  void dispatch(Args &&...);

  template <typename... Args>
  static void dispatch_helper(auto &self, Args &&...);

  Account &get_account(std::string_view const &account);

  template <typename Res, typename T, typename... Args>
  Res dispatch_order_action(Trace<T> const &, Args &&...);

 private:
  server::Dispatcher &dispatcher_;
  // accounts
  utils::unordered_map<std::string, std::unique_ptr<Account>> const accounts_;
  // io
  io::Context &context_;
  // shared
  Shared shared_;
  // seed
  uint16_t stream_id_ = {};
  // streams
  Rest rest_;
  std::vector<std::unique_ptr<MarketData>> market_data_1_, market_data_2_;
  utils::unordered_map<std::string, Request> request_;
  utils::unordered_map<std::string, std::unique_ptr<server::OrderActionStream>> order_entry_;
  utils::unordered_map<std::string, std::unique_ptr<server::Stream>> drop_copy_;
  utils::unordered_map<std::string, std::unique_ptr<server::OrderActionStream>> order_entry_margin_;
  utils::unordered_map<std::string, std::unique_ptr<DropCopyMargin>> drop_copy_margin_;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
