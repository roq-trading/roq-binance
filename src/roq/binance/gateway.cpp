/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/gateway.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

#include "roq/logging.hpp"

#include "roq/clock.hpp"

#include "roq/server/oms/exceptions.hpp"

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
  for (auto &[_, account] : config.accounts) {
    auto obj = std::make_unique<Account>(config, account.name, account.margin_mode);
    result.try_emplace(static_cast<std::string_view>(account.name), std::move(obj));
  }
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
R create_order_entry(auto &gateway, auto &context, auto &stream_id, auto &accounts, auto &shared, auto &request_by_account) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[_, item] : accounts) {
    auto &account = *item;
    auto &request = request_by_account[account.name];
    std::vector<std::unique_ptr<OrderEntry>> order_entry;
    if (shared.settings.ws_api) {
      auto &interfaces = shared.settings.ws_api_2.network_interfaces;
      if (std::empty(interfaces)) {
        auto obj = std::make_unique<OrderEntryWS>(gateway, context, ++stream_id, account, shared, request);
        order_entry.emplace_back(std::move(obj));
      } else {
        for (size_t i = 0; i < std::size(interfaces); ++i) {
          auto master = i == 0;
          auto &interface = interfaces[i];
          auto obj = std::make_unique<OrderEntryWS>(gateway, context, ++stream_id, account, shared, request, master, interface);
          order_entry.emplace_back(std::move(obj));
        }
      }
    } else {
      auto &interfaces = shared.settings.rest.network_interfaces;
      if (std::empty(interfaces)) {
        auto obj = std::make_unique<OrderEntryREST>(gateway, context, ++stream_id, account, shared, request);
        order_entry.emplace_back(std::move(obj));
      } else {
        for (size_t i = 0; i < std::size(interfaces); ++i) {
          auto master = i == 0;
          auto &interface = interfaces[i];
          auto obj = std::make_unique<OrderEntryREST>(gateway, context, ++stream_id, account, shared, request, master, interface);
          order_entry.emplace_back(std::move(obj));
        }
      }
    }
    result.try_emplace(account.name, std::move(order_entry));
  }
  return result;
}

template <typename R>
R create_drop_copy(auto &accounts) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[_, item] : accounts) {
    auto &account = *item;
    result.try_emplace(account.name, nullptr);
  }
  return result;
}

template <typename R>
R create_order_entry_portfolio(auto &gateway, auto &context, auto &stream_id, auto &accounts, auto &shared, auto &request_by_account) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[_, item] : accounts) {
    auto &account = *item;
    if (account.margin_mode != MarginMode::PORTFOLIO)
      continue;
    auto &request = request_by_account[account.name];
    auto obj = std::make_unique<OrderEntryPortfolio>(gateway, context, ++stream_id, account, shared, request);
    result.try_emplace(account.name, std::move(obj));
  }
  return result;
}

template <typename R>
R create_drop_copy_portfolio(auto &accounts) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[_, item] : accounts) {
    auto &account = *item;
    if (account.margin_mode == MarginMode::PORTFOLIO)
      result.try_emplace(account.name, nullptr);
  }
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

Gateway::Gateway(server::Dispatcher &dispatcher, Settings const &settings, Config const &config, io::Context &context)
    : dispatcher_{dispatcher}, accounts_(create_accounts<decltype(accounts_)>(config)), context_{context}, shared_{dispatcher, settings, config},
      requests_{create_request<decltype(requests_)>(config)}, requests_portfolio_{create_request<decltype(requests_portfolio_)>(config)},
      rest_{*this, context_, ++stream_id_, shared_},
      order_entry_{create_order_entry<decltype(order_entry_)>(*this, context_, stream_id_, accounts_, shared_, requests_)},
      drop_copy_{create_drop_copy<decltype(drop_copy_)>(accounts_)}, order_entry_portfolio_{create_order_entry_portfolio<decltype(order_entry_portfolio_)>(
                                                                         *this, context_, stream_id_, accounts_, shared_, requests_portfolio_)},
      drop_copy_portfolio_{create_drop_copy_portfolio<decltype(drop_copy_portfolio_)>(accounts_)} {
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

void Gateway::operator()(Trace<TradeUpdate> const &event, bool is_last, uint8_t user_id, std::string_view const &request_id) {
  dispatcher_(event, is_last, user_id, request_id);
}

void Gateway::operator()(Trace<FundsUpdate> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Rest::SymbolsUpdate &symbols_update) {
  auto [size, start_from] = shared_.symbols(symbols_update.symbols);
  ensure_symbol_slices(size);
  for (auto &item : market_data_1_)
    (*item).subscribe(start_from);
  for (auto &item : market_data_2_)
    (*item).subscribe(start_from);
}

void Gateway::ensure_symbol_slices(size_t size) {
  auto helper = [&](auto &container, auto priority) {
    auto stream_id = ++stream_id_;
    auto index = std::size(container);
    log::info("Create MarketData (stream_id={}, priority={}, index={}))"sv, stream_id, priority, index);
    auto market_data = std::make_unique<MarketData>(*this, context_, stream_id, priority, shared_, index);
    MessageInfo message_info;
    Start start;
    create_event_and_dispatch(*market_data, message_info, start);
    container.emplace_back(std::move(market_data));
  };
  while (std::size(market_data_1_) < size)
    helper(market_data_1_, Priority::PRIMARY);
  if (!shared_.settings.ws.enable_secondary)
    return;  // note
  while (std::size(market_data_2_) < size)
    helper(market_data_2_, Priority::SECONDARY);
}

void Gateway::operator()(OrderEntry::ListenKeyUpdate const &listen_key_update) {
  switch (listen_key_update.margin_mode) {
    using enum MarginMode;
    case UNDEFINED:
    case ISOLATED:
    case CROSS:
      create_drop_copy_from_listen_key_update<DropCopySimple>(drop_copy_, listen_key_update);
      break;
    case PORTFOLIO:
      create_drop_copy_from_listen_key_update<DropCopyPortfolio>(drop_copy_portfolio_, listen_key_update);
      break;
  }
}

template <typename T>
void Gateway::create_drop_copy_from_listen_key_update(auto &drop_copy, auto &listen_key_update) {
  auto &account = listen_key_update.account;
  auto iter = drop_copy.find(account);
  if (iter == std::end(drop_copy))
    log::fatal(R"(Unexpected: account="{}")"sv, account);
  if (!static_cast<bool>((*iter).second)) {
    log::info(R"(Create drop-copy (user-stream) for account="{}", margin_mode={})"sv, account, listen_key_update.margin_mode);
    auto &account_2 = get_account(account);
    auto &request = get_request(account, listen_key_update.margin_mode);
    auto obj = std::make_unique<T>(*this, context_, ++stream_id_, account_2, shared_, request, listen_key_update.listen_key);
    MessageInfo message_info;
    Start start;
    create_event_and_dispatch(*obj, message_info, start);
    (*iter).second = std::move(obj);
  }
}

uint16_t Gateway::operator()(Event<CreateOrder> const &event, server::oms::Order const &order, std::string_view const &request_id) {
  auto &create_order = event.value;
  assert(!std::empty(create_order.account));
  return get_order_entry(create_order.account, create_order.margin_mode)(event, order, request_id);
}

uint16_t Gateway::operator()(
    Event<ModifyOrder> const &event, server::oms::Order const &order, std::string_view const &request_id, std::string_view const &previous_request_id) {
  auto &modify_order = event.value;
  assert(!std::empty(modify_order.account));
  assert(modify_order.account == order.account);
  return get_order_entry(modify_order.account, order.margin_mode)(event, order, request_id, previous_request_id);
}

uint16_t Gateway::operator()(
    Event<CancelOrder> const &event, server::oms::Order const &order, std::string_view const &request_id, std::string_view const &previous_request_id) {
  auto &cancel_order = event.value;
  assert(!std::empty(cancel_order.account));
  assert(cancel_order.account == order.account);
  return get_order_entry(cancel_order.account, order.margin_mode)(event, order, request_id, previous_request_id);
}

uint16_t Gateway::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  auto &cancel_all_orders = event.value;
  assert(!std::empty(cancel_all_orders.account));
  auto &account = get_account(cancel_all_orders.account);
  if (account.margin_mode == MarginMode::PORTFOLIO)
    get_order_entry(cancel_all_orders.account, account.margin_mode)(event, request_id);
  return get_order_entry(cancel_all_orders.account, {})(event, request_id);
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
  for (auto &[_, item] : order_entry_portfolio_)
    helper(*item);
  for (auto &[_, item] : drop_copy_portfolio_)
    if (static_cast<bool>(item))
      helper(*item);
  for (auto &item : market_data_1_)
    helper(*item);
  for (auto &item : market_data_2_)
    helper(*item);
}

Account &Gateway::get_account(std::string_view const &account) {
  auto iter = accounts_.find(account);
  if (iter == std::end(accounts_)) [[unlikely]]
    throw RuntimeError{R"(Unknown account="{}")"sv, account};
  return *(*iter).second;
}

Request &Gateway::get_request(std::string_view const &account, MarginMode margin_mode) {
  switch (margin_mode) {
    using enum MarginMode;
    case UNDEFINED:
    case ISOLATED:
    case CROSS: {
      auto iter = requests_.find(account);
      if (iter == std::end(requests_)) [[unlikely]]
        throw RuntimeError{R"(Unknown account="{}")"sv, account};
      return (*iter).second;
    }
    case PORTFOLIO: {
      auto iter = requests_portfolio_.find(account);
      if (iter == std::end(requests_portfolio_)) [[unlikely]]
        throw RuntimeError{R"(Unknown account="{}")"sv, account};
      return (*iter).second;
    }
  }
  log::fatal("Unexpected"sv);
}

OrderEntry &Gateway::get_order_entry(std::string_view const &account, MarginMode margin_mode) {
  switch (margin_mode) {
    using enum MarginMode;
    case UNDEFINED:
    case ISOLATED:
    case CROSS: {
      auto iter = order_entry_.find(account);
      if (iter == std::end(order_entry_)) [[unlikely]]
        throw RuntimeError{R"(Unknown account="{}")"sv, account};
      return (*iter).second.get_next();
    }
    case PORTFOLIO: {
      auto iter = order_entry_portfolio_.find(account);
      if (iter == std::end(order_entry_portfolio_)) [[unlikely]]
        throw RuntimeError{R"(Unknown account="{}")"sv, account};
      return *(*iter).second;
    }
  }
  log::fatal("Unexpected"sv);
}

DropCopy &Gateway::get_drop_copy(std::string_view const &account, MarginMode margin_mode) {
  auto helper = [&](auto &drop_copy) -> auto & {
    auto iter = drop_copy.find(account);
    if (iter == std::end(drop_copy)) [[unlikely]]
      throw RuntimeError{R"(Unknown account="{}")"sv, account};
    return *(*iter).second;
  };
  switch (margin_mode) {
    using enum MarginMode;
    case UNDEFINED:
    case ISOLATED:
    case CROSS:
      return helper(drop_copy_);
    case PORTFOLIO:
      return helper(drop_copy_portfolio_);
  }
  log::fatal("Unexpected"sv);
}

Gateway::OrderEntryRR::OrderEntryRR(std::vector<std::unique_ptr<OrderEntry>> &&order_entry) : order_entry_{std::move(order_entry)} {
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
    if (!order_entry.ready())
      continue;
    index_ = (index + 1) % length;
    return order_entry;
  }
  throw server::oms::NotReady{"get_next"sv};
}
}  // namespace binance
}  // namespace roq
