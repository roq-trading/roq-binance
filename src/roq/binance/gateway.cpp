/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/gateway.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

#include "roq/clock.hpp"

#include "roq/logging.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/core/charconv.hpp"
#include "roq/core/utils.hpp"

#include "roq/binance/drop_copy_portfolio.hpp"
#include "roq/binance/drop_copy_simple.hpp"

#include "roq/binance/order_entry_portfolio.hpp"
#include "roq/binance/order_entry_rest.hpp"
#include "roq/binance/order_entry_ws.hpp"

#include "roq/binance/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === HELPERS ===

namespace {
template <typename R>
R create_accounts(auto &config) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[_, account] : config.accounts)
    result.try_emplace(
        static_cast<std::string_view>(account.name),
        std::make_unique<Account>(config, account.name, account.margin_mode));
  return result;
}

template <typename R>
R create_request(auto &config) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[_, account] : config.accounts)
    result.try_emplace(static_cast<std::string_view>(account.name), Request{});
  return result;
}

template <typename R>
R create_order_entry(
    auto &gateway, auto &context, auto &stream_id, auto &accounts, auto &shared, auto &request_by_account) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[name, account] : accounts) {
    auto &request = request_by_account[name];
    std::vector<std::unique_ptr<OrderEntry>> order_entry;
    switch ((*account).margin_mode) {
      using enum MarginMode;
      case UNDEFINED:
      case ISOLATED:
      case CROSS:
        if (shared.settings.ws_api) {
          auto &interfaces = shared.settings.ws_api_2.network_interfaces;
          if (std::empty(interfaces)) {
            order_entry.emplace_back(
                std::make_unique<OrderEntryWS>(gateway, context, ++stream_id, *account, shared, request));
          } else {
            for (size_t i = 0; i < std::size(interfaces); ++i) {
              auto master = i == 0;
              auto &interface = interfaces[i];
              order_entry.emplace_back(std::make_unique<OrderEntryWS>(
                  gateway, context, ++stream_id, *account, shared, request, master, interface));
            }
          }
        } else {
          auto &interfaces = shared.settings.rest.network_interfaces;
          if (std::empty(interfaces)) {
            order_entry.emplace_back(
                std::make_unique<OrderEntryREST>(gateway, context, ++stream_id, *account, shared, request));
          } else {
            for (size_t i = 0; i < std::size(interfaces); ++i) {
              auto master = i == 0;
              auto &interface = interfaces[i];
              order_entry.emplace_back(std::make_unique<OrderEntryREST>(
                  gateway, context, ++stream_id, *account, shared, request, master, interface));
            }
          }
        }
        break;
      case PORTFOLIO: {
        auto &interfaces = shared.settings.rest.network_interfaces;
        if (std::empty(interfaces)) {
          order_entry.emplace_back(
              std::make_unique<OrderEntryPortfolio>(gateway, context, ++stream_id, *account, shared, request));
        } else {
          for (size_t i = 0; i < std::size(interfaces); ++i) {
            auto master = i == 0;
            auto &interface = interfaces[i];
            order_entry.emplace_back(std::make_unique<OrderEntryPortfolio>(
                gateway, context, ++stream_id, *account, shared, request, master, interface));
          }
        }
        break;
      }
    }
    result.try_emplace(name, std::move(order_entry));
  }
  return result;
}

template <typename R>
R create_drop_copy(auto &accounts) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[name, account] : accounts)
    result.try_emplace(name, nullptr);
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

Gateway::Gateway(server::Dispatcher &dispatcher, Settings const &settings, Config const &config, io::Context &context)
    : dispatcher_{dispatcher}, accounts_(create_accounts<decltype(accounts_)>(config)), context_{context},
      shared_{dispatcher, settings, config}, request_{create_request<decltype(request_)>(config)},
      rest_{*this, context_, ++stream_id_, shared_}, order_entry_{create_order_entry<decltype(order_entry_)>(
                                                         *this, context_, stream_id_, accounts_, shared_, request_)},
      drop_copy_{create_drop_copy<decltype(drop_copy_)>(accounts_)} {
  if (settings.rest.cancel_on_disconnect)
    log::fatal("Exchange does *NOT* support cancel on disconnect"sv);
}

void Gateway::operator()(Event<Start> const &event) {
  log::info("Starting..."sv);
  assert(std::empty(market_data_1_));
  assert(std::empty(market_data_2_));
  dispatch(event);
}

void Gateway::operator()(Event<Stop> const &event) {
  log::info("Stopping..."sv);
  dispatch(event);
}

void Gateway::operator()(Event<Timer> const &event) {
  dispatch(event);
}

void Gateway::operator()(Event<Connected> const &) {
}

void Gateway::operator()(Event<Disconnected> const &) {
}

void Gateway::operator()(Trace<StreamStatus> const &event) {
  dispatcher_(event);
}

void Gateway::operator()(Trace<ExternalLatency> const &event) {
  dispatcher_(event);
}

void Gateway::operator()(Trace<RateLimitsUpdate> const &event) {
  dispatcher_(event);
}

void Gateway::operator()(Trace<ReferenceData> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<MarketStatus> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<TopOfBook> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<MarketByPriceUpdate> const &event, bool is_last) {
  auto callback = []([[maybe_unused]] auto &market_by_price) {};
  dispatcher_(event, is_last, bids_, asks_, callback);
}

void Gateway::operator()(Trace<TradeSummary> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<StatisticsUpdate> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(
    Trace<TradeUpdate> const &event, bool is_last, uint8_t user_id, std::string_view const &request_id) {
  dispatcher_(event, is_last, user_id, request_id);
}

void Gateway::operator()(Trace<FundsUpdate> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Rest::SymbolsUpdate &symbols_update) {
  auto [size, start_from] = shared_.symbols(symbols_update.symbols);
  ensure_symbol_slices(size);
  for (auto &iter : market_data_1_)
    (*iter).subscribe(start_from);
  for (auto &iter : market_data_2_)
    (*iter).subscribe(start_from);
}

void Gateway::ensure_symbol_slices(size_t size) {
  while (std::size(market_data_1_) < size) {
    auto stream_id = ++stream_id_;
    auto index = std::size(market_data_1_);
    log::info("Create MarketData (stream_id={}, index={}))"sv, stream_id, index);
    auto market_data = std::make_unique<MarketData>(*this, context_, stream_id, Priority::PRIMARY, shared_, index);
    MessageInfo message_info;
    Start start;
    create_event_and_dispatch(*market_data, message_info, start);
    market_data_1_.emplace_back(std::move(market_data));
  }
  if (!shared_.settings.ws.enable_secondary)
    return;
  while (std::size(market_data_2_) < size) {
    auto stream_id = ++stream_id_;
    auto index = std::size(market_data_2_);
    log::info("Create MarketData (stream_id={}, index={}))"sv, stream_id, index);
    auto market_data = std::make_unique<MarketData>(*this, context_, stream_id, Priority::SECONDARY, shared_, index);
    MessageInfo message_info;
    Start start;
    create_event_and_dispatch(*market_data, message_info, start);
    market_data_2_.emplace_back(std::move(market_data));
  }
}

void Gateway::operator()(OrderEntry::ListenKeyUpdate const &listen_key_update) {
  auto &account = listen_key_update.account;
  assert(!std::empty(account));
  auto iter = drop_copy_.find(account);
  if (iter == std::end(drop_copy_))
    log::fatal(R"(Unexpected: account="{}")"sv, account);
  if (!static_cast<bool>((*iter).second)) {
    log::info(R"(Create drop-copy (user-stream) for account="{}")"sv, account);
    auto &account_2 = *accounts_.at(account);
    std::unique_ptr<DropCopy> drop_copy;
    switch (account_2.margin_mode) {
      using enum MarginMode;
      case UNDEFINED:
      case ISOLATED:
      case CROSS:
        drop_copy = std::make_unique<DropCopySimple>(
            *this, context_, ++stream_id_, account_2, shared_, request_.at(account), listen_key_update.listen_key);
        break;
      case PORTFOLIO:
        drop_copy = std::make_unique<DropCopyPortfolio>(
            *this, context_, ++stream_id_, account_2, shared_, request_.at(account), listen_key_update.listen_key);
        break;
    }
    MessageInfo message_info;
    Start start;
    create_event_and_dispatch(*drop_copy, message_info, start);
    (*iter).second = std::move(drop_copy);
  }
}

uint16_t Gateway::operator()(
    Event<CreateOrder> const &event, server::oms::Order const &order, std::string_view const &request_id) {
  assert(!std::empty(event.value.account));
  return get_order_entry(event.value.account)(event, order, request_id);
}

uint16_t Gateway::operator()(
    Event<ModifyOrder> const &event,
    server::oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  assert(!std::empty(event.value.account));
  assert(event.value.account == order.account);
  return get_order_entry(event.value.account)(event, order, request_id, previous_request_id);
}

uint16_t Gateway::operator()(
    Event<CancelOrder> const &event,
    server::oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  assert(!std::empty(event.value.account));
  assert(event.value.account == order.account);
  return get_order_entry(event.value.account)(event, order, request_id, previous_request_id);
}

uint16_t Gateway::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  assert(!std::empty(event.value.account));
  return get_order_entry(event.value.account)(event, request_id);
}

void Gateway::operator()(metrics::Writer &writer) {
  dispatch(writer);
}

template <typename... Args>
void Gateway::dispatch(Args &&...args) {
  auto helper = [&](auto &target) { target(std::forward<Args>(args)...); };
  helper(rest_);
  for (auto &[_, item] : order_entry_)
    helper(item);
  for (auto &[_, item] : drop_copy_)
    if (static_cast<bool>(item))
      helper(*item);
  for (auto &item : market_data_1_)
    helper(*item);
  for (auto &item : market_data_2_)
    helper(*item);
}

OrderEntry &Gateway::get_order_entry(std::string_view const &account) {
  auto iter = order_entry_.find(account);
  if (iter == std::end(order_entry_)) [[unlikely]]
    throw RuntimeError{R"(Unknown account="{}")"sv, account};
  return (*iter).second.get_next();
}

Gateway::OrderEntryRR::OrderEntryRR(std::vector<std::unique_ptr<OrderEntry>> &&order_entry)
    : order_entry_{std::move(order_entry)} {
  for (auto &item : order_entry_)
    if (item.get() == nullptr)
      log::fatal("HERE"sv);
}

template <typename... Args>
void Gateway::OrderEntryRR::operator()(Args &&...args) {
  for (auto &item : order_entry_)
    (*item)(std::forward<Args>(args)...);
}

OrderEntry &Gateway::OrderEntryRR::get_next() {
  auto length = std::size(order_entry_);
  for (size_t offset = 0; offset < length; ++offset) {
    auto index = (index_ + offset) % length;
    auto &order_entry = *(order_entry_[index]);
    if (!order_entry.ready()) {
      continue;
    }
    index_ = (index + 1) % length;
    return order_entry;
  }
  throw server::oms::NotReady{"get_next"sv};
}

}  // namespace binance
}  // namespace roq
