/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/gateway.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

#include "roq/clock.hpp"

#include "roq/logging.hpp"

#include "roq/core/charconv.hpp"
#include "roq/core/utils.hpp"

#include "roq/binance/flags.hpp"

#include "roq/binance/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === HELPERS ===

namespace {
template <typename R>
auto create_authenticator(auto const &config) {
  R result;
  for (auto &[_, account] : config.accounts)
    result.try_emplace(account.name, std::make_unique<Authenticator>(config, account.name));
  return result;
}

template <typename R>
auto create_request(auto const &config) {
  R result;
  for (auto &[_, account] : config.accounts)
    result.try_emplace(account.name, Request{});
  return result;
}

template <typename R>
auto create_order_entry(
    auto &gateway, auto &context, auto &stream_id, auto &security_by_account, auto &shared, auto &request_by_account) {
  R result;
  for (auto &[account, security] : security_by_account) {
    auto &request = request_by_account[account];
    result.try_emplace(
        account, std::make_unique<OrderEntry>(gateway, context, ++stream_id, *security, shared, request));
  }
  return result;
}

template <typename R>
auto create_order_entry_ws(auto &gateway, auto &context, auto &stream_id, auto &security_by_account, auto &shared) {
  R result;
  for (auto &[account, security] : security_by_account)
    result.try_emplace(account, std::make_unique<OrderEntryWS>(gateway, context, ++stream_id, *security, shared));
  return result;
}

template <typename R>
auto create_drop_copy(auto &security_by_account) {
  R result;
  for (auto &[account, security] : security_by_account)
    result.try_emplace(account, nullptr);
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

Gateway::Gateway(server::Dispatcher &dispatcher, Config const &config, io::Context &context)
    : dispatcher_{dispatcher}, ws_api_{flags::Flags::ws_api()},
      authenticator_(create_authenticator<decltype(authenticator_)>(config)), context_{context}, shared_{dispatcher},
      request_{create_request<decltype(request_)>(config)}, rest_{*this, context_, ++stream_id_, shared_},
      order_entry_{
          create_order_entry<decltype(order_entry_)>(*this, context_, stream_id_, authenticator_, shared_, request_)},
      order_entry_ws_{
          create_order_entry_ws<decltype(order_entry_ws_)>(*this, context_, stream_id_, authenticator_, shared_)},
      drop_copy_{create_drop_copy<decltype(drop_copy_)>(authenticator_)} {
  if (Flags::rest_cancel_on_disconnect())
    log::fatal("Exchange does *NOT* support cancel on disconnect"sv);
}

void Gateway::operator()(Event<Start> const &event) {
  log::info("Starting..."sv);
  rest_(event);
  for (auto &[_, iter] : order_entry_)
    (*iter)(event);
  for (auto &[_, iter] : order_entry_ws_)
    (*iter)(event);
  for (auto &[_, iter] : drop_copy_)
    if (static_cast<bool>(iter))
      (*iter)(event);
  assert(std::empty(market_data_1_));
  assert(std::empty(market_data_2_));
  // order_entry_.download.begin();
}

void Gateway::operator()(Event<Stop> const &event) {
  log::info("Stopping..."sv);
  for (auto &iter : market_data_2_)
    (*iter)(event);
  for (auto &iter : market_data_1_)
    (*iter)(event);
  for (auto &[_, iter] : drop_copy_)
    if (static_cast<bool>(iter))
      (*iter)(event);
  for (auto &[_, iter] : order_entry_ws_)
    (*iter)(event);
  for (auto &[_, iter] : order_entry_)
    (*iter)(event);
  rest_(event);
}

void Gateway::operator()(Event<Timer> const &event) {
  rest_(event);
  for (auto &[_, iter] : order_entry_)
    (*iter)(event);
  for (auto &[_, iter] : order_entry_ws_)
    (*iter)(event);
  for (auto &[_, iter] : drop_copy_)
    if (static_cast<bool>(iter))
      (*iter)(event);
  for (auto &iter : market_data_1_)
    (*iter)(event);
  for (auto &iter : market_data_2_)
    (*iter)(event);
}

void Gateway::operator()(Event<Connected> const &) {
}

void Gateway::operator()(Event<Disconnected> const &event) {
  auto const &[message_info, disconnected] = event;
  log::warn(
      R"(Disconnected: source="{}", order_cancel_policy={})"sv,
      message_info.source_name,
      disconnected.order_cancel_policy);
  for (auto &[_, iter] : order_entry_)
    (*iter)(event);
  for (auto &[_, iter] : order_entry_ws_)
    (*iter)(event);
  switch (disconnected.order_cancel_policy) {
    using enum OrderCancelPolicy;
    case UNDEFINED:
      break;
    case MANAGED_ORDERS:
      log::warn("*** CANCEL MANAGED ORDERS NOT IMPLEMENTED ***"sv);
      break;
    case BY_ACCOUNT:
      log::warn("*** CANCEL ALL ACCOUNT ORDERS ***"sv);
      for (auto &[account, order_entry] : order_entry_) {
        if (dispatcher_.can_user_trade_account(account, message_info.source)) {
          log::warn(R"(- account="{}")"sv, account);
          CancelAllOrders cancel_all_orders{
              .account = account,
          };
          Event event{message_info, cancel_all_orders};
          (*order_entry)(event, {});
        }
      }
      for (auto &[account, order_entry] : order_entry_ws_) {
        if (dispatcher_.can_user_trade_account(account, message_info.source)) {
          log::warn(R"(- account="{}")"sv, account);
          CancelAllOrders cancel_all_orders{
              .account = account,
          };
          Event event{message_info, cancel_all_orders};
          (*order_entry)(event, {});
        }
      }
  }
  for (auto &[account, order_entry] : order_entry_)
    (*order_entry)(event);
}

void Gateway::operator()(Trace<StreamStatus> const &event) {
  dispatcher_(event);
}

void Gateway::operator()(Trace<ExternalLatency> const &event) {
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

void Gateway::operator()(Trace<oms::TradeUpdate> const &event, uint16_t stream_id, bool is_last, uint8_t user_id) {
  dispatcher_(event, stream_id, is_last, user_id);
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
    log::debug("Create MarketData (stream_id={}, index={}))"sv, stream_id, index);
    auto market_data = std::make_unique<MarketData>(*this, context_, stream_id, Priority::PRIMARY, shared_, index);
    MessageInfo message_info;
    Start start;
    create_event_and_dispatch(*market_data, message_info, start);
    market_data_1_.emplace_back(std::move(market_data));
  }
  if (!Flags::ws_enable_secondary())
    return;
  while (std::size(market_data_2_) < size) {
    auto stream_id = ++stream_id_;
    auto index = std::size(market_data_2_);
    log::debug("Create MarketData (stream_id={}, index={}))"sv, stream_id, index);
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
  if (iter == std::end(drop_copy_)) {
    log::fatal(R"(Unexpected: account="{}")"sv, account);
  } else if (!static_cast<bool>((*iter).second)) {
    log::info(R"(Create drop-copy (user-stream) for account="{}")"sv, account);
    auto drop_copy = std::make_unique<DropCopy>(
        *this,
        context_,
        ++stream_id_,
        *authenticator_[account],
        shared_,
        request_[account],
        listen_key_update.listen_key);
    MessageInfo message_info;
    Start start;
    create_event_and_dispatch(*drop_copy, message_info, start);
    (*iter).second = std::move(drop_copy);
  }
}

uint16_t Gateway::operator()(
    Event<CreateOrder> const &event, oms::Order const &order, std::string_view const &request_id) {
  assert(!std::empty(event.value.account));
  if (ws_api_) {
    return get_order_entry_ws(event.value.account)(event, order, request_id);
  } else {
    return get_order_entry(event.value.account)(event, order, request_id);
  }
}

uint16_t Gateway::operator()(
    Event<ModifyOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  assert(!std::empty(event.value.account));
  assert(event.value.account == order.account);
  if (ws_api_) {
    return get_order_entry_ws(event.value.account)(event, order, request_id, previous_request_id);
  } else {
    return get_order_entry(event.value.account)(event, order, request_id, previous_request_id);
  }
}

uint16_t Gateway::operator()(
    Event<CancelOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  assert(!std::empty(event.value.account));
  assert(event.value.account == order.account);
  if (ws_api_) {
    return get_order_entry_ws(event.value.account)(event, order, request_id, previous_request_id);
  } else {
    return get_order_entry(event.value.account)(event, order, request_id, previous_request_id);
  }
}

uint16_t Gateway::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  assert(!std::empty(event.value.account));
  if (ws_api_) {
    return get_order_entry_ws(event.value.account)(event, request_id);
  } else {
    return get_order_entry(event.value.account)(event, request_id);
  }
}

void Gateway::operator()(metrics::Writer &writer) {
  for (auto &[_, iter] : order_entry_)
    (*iter)(writer);
  for (auto &[_, iter] : order_entry_ws_)
    (*iter)(writer);
  for (auto &[_, iter] : drop_copy_)
    if (static_cast<bool>(iter))
      (*iter)(writer);
  for (auto &iter : market_data_1_)
    (*iter)(writer);
  for (auto &iter : market_data_2_)
    (*iter)(writer);
}

OrderEntry &Gateway::get_order_entry(std::string_view const &account) {
  auto iter = order_entry_.find(account);
  if (iter != std::end(order_entry_))
    return *(*iter).second;
  throw RuntimeError{R"(Unknown account="{}")"sv, account};
}

OrderEntryWS &Gateway::get_order_entry_ws(std::string_view const &account) {
  auto iter = order_entry_ws_.find(account);
  if (iter != std::end(order_entry_ws_))
    return *(*iter).second;
  throw RuntimeError{R"(Unknown account="{}")"sv, account};
}

}  // namespace binance
}  // namespace roq
