/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/gateway.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

#include "roq/logging.hpp"

#include "roq/clock.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/binance/drop_copy_portfolio.hpp"
#include "roq/binance/order_entry_margin.hpp"
#include "roq/binance/order_entry_portfolio.hpp"
#include "roq/binance/web_socket.hpp"

#include "roq/binance/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === HELPERS ===

namespace {
template <typename R>
R create_accounts(auto &config) {
  using result_type = std::remove_cvref_t<R>;
  result_type result;
  for (auto &[_, account] : config.accounts) {
    auto obj = std::make_unique<Account>(config, account.name, account.margin_mode);
    result.try_emplace(static_cast<std::string_view>(account.name), std::move(obj));
  }
  return result;
}

template <typename R>
R create_request(auto &config) {
  using result_type = std::remove_cvref_t<R>;
  result_type result;
  for (auto &[_, account] : config.accounts) {
    result.try_emplace(static_cast<std::string_view>(account.name), Request{});
  }
  return result;
}

template <typename R>
R create_order_entry(auto &gateway, auto &context, auto &stream_id, auto &accounts, auto &shared, auto &request) {
  using result_type = std::remove_cvref_t<R>;
  result_type result;
  for (auto &[_, item] : accounts) {
    auto &account = *item;
    switch (account.margin_mode) {
      using enum MarginMode;
      case UNDEFINED:
      case ISOLATED:
      case CROSS: {
        auto web_socket = std::make_unique<WebSocket>(gateway, context, ++stream_id, account, shared);
        result.try_emplace(account.name, std::move(web_socket));
        break;
      }
      case PORTFOLIO: {
        auto order_entry_portfolio = std::make_unique<OrderEntryPortfolio>(gateway, context, ++stream_id, account, shared, request[account.name]);
        result.try_emplace(account.name, std::move(order_entry_portfolio));
        break;
      }
    }
  }
  return result;
}

template <typename R>
R create_order_entry_margin(auto &gateway, auto &settings, auto &context, auto &stream_id, auto &accounts, auto &shared, auto &request) {
  using result_type = std::remove_cvref_t<R>;
  result_type result;
  if (!settings.misc.experimental_drop_margin) {
    for (auto &[_, item] : accounts) {
      auto &account = *item;
      if (account.margin_mode != MarginMode::PORTFOLIO) {
        auto order_entry = std::make_unique<OrderEntryMargin>(gateway, context, ++stream_id, account, shared, request[account.name]);
        result.try_emplace(account.name, std::move(order_entry));
      }
    }
  }
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

Gateway::Gateway(server::Dispatcher &dispatcher, Settings const &settings, Config const &config, io::Context &context)
    : dispatcher_{dispatcher}, accounts_(create_accounts<decltype(accounts_)>(config)), context_{context}, shared_{dispatcher, settings, config},
      rest_{*this, context_, ++stream_id_, shared_}, request_{create_request<decltype(request_)>(config)},
      order_entry_{create_order_entry<decltype(order_entry_)>(*this, context_, stream_id_, accounts_, shared_, request_)},
      order_entry_margin_{create_order_entry_margin<decltype(order_entry_)>(*this, settings, context_, stream_id_, accounts_, shared_, request_)} {
  if (settings.rest.cancel_on_disconnect) {
    log::fatal("Exchange does *NOT* support cancel on disconnect"sv);
  }
}

// server::Handler

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

void Gateway::operator()(Event<Control> const &event) {
  auto &[message_info, control] = event;
  switch (control.action) {
    using enum Action;
    case UNDEFINED:
      assert(false);
      break;
    case ENABLE:
      dispatcher_(State::ENABLED);
      break;
    case DISABLE:
      dispatcher_(State::DISABLED);
      break;
  }
}

void Gateway::operator()(Event<Connected> const &) {
}

void Gateway::operator()(Event<Disconnected> const &) {
}

void Gateway::operator()(Event<Subscribe> const &event) {
  auto &[message_info, subscribe] = event;
  std::vector<Symbol> symbols;
  for (auto &item : subscribe.symbols) {
    if (shared_.all_symbols.emplace(item).second) {
      symbols.emplace_back(item);
    } else {
      log::warn(R"(*** DUPLICATE SUBSCRIPTION *** (symbol="{}")"sv, item);
    }
  }
  auto symbols_update = Rest::SymbolsUpdate{
      .symbols = symbols,
  };
  (*this)(symbols_update);
}

uint16_t Gateway::operator()(
    Event<CreateOrder> const &event, server::oms::Order const &order, server::oms::RefData const &ref_data, std::string_view const &request_id) {
  auto &[message_info, create_order] = event;
  assert(!std::empty(create_order.account));
  return get_order_entry(create_order.account, create_order.margin_mode)(event, order, ref_data, request_id);
}

uint16_t Gateway::operator()(
    Event<ModifyOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto &[message_info, modify_order] = event;
  assert(!std::empty(modify_order.account));
  assert(modify_order.account == order.account);
  return get_order_entry(modify_order.account, order.margin_mode)(event, order, ref_data, request_id, previous_request_id);
}

uint16_t Gateway::operator()(
    Event<CancelOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto &[message_info, cancel_order] = event;
  assert(!std::empty(cancel_order.account));
  assert(cancel_order.account == order.account);
  return get_order_entry(cancel_order.account, order.margin_mode)(event, order, ref_data, request_id, previous_request_id);
}

uint16_t Gateway::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  auto &[message_info, cancel_all_orders] = event;
  assert(!std::empty(cancel_all_orders.account));
  auto helper = [&](auto &lookup) {
    auto iter = lookup.find(cancel_all_orders.account);
    if (iter != std::end(lookup)) {
      (*(*iter).second)(event, request_id);
    }
  };
  helper(order_entry_);
  helper(order_entry_margin_);
  return {};
}

uint16_t Gateway::operator()(Event<MassQuote> const &) {
  throw server::oms::NotSupported{"not supported"sv};
}

uint16_t Gateway::operator()(Event<CancelQuotes> const &) {
  throw server::oms::NotSupported{"not supported"sv};
}

void Gateway::operator()(metrics::Writer &writer) const {
  dispatch_helper(*this, writer);
}

// streams

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

void Gateway::operator()(OrderEntry::ListenKeyUpdate const &listen_key_update) {
  switch (listen_key_update.margin_mode) {
    using enum MarginMode;
    case UNDEFINED:
      log::fatal("Unexpected"sv);
    case ISOLATED:
    case CROSS: {
      auto &instance = drop_copy_margin_[listen_key_update.account];
      if (instance) {
        (*instance)(listen_key_update);
      } else {
        log::info(R"(Create drop-copy (user-stream) for account="{}", margin_mode={})"sv, listen_key_update.account, listen_key_update.margin_mode);
        auto &account_2 = get_account(listen_key_update.account);
        auto &request = request_[listen_key_update.account];  // XXX
        instance = std::make_unique<DropCopyMargin>(*this, context_, ++stream_id_, account_2, shared_, request, listen_key_update);
        MessageInfo message_info;
        Start start;
        create_event_and_dispatch(*instance, message_info, start);
      }
      break;
    }
    case PORTFOLIO: {
      auto &instance = drop_copy_[listen_key_update.account];
      if (!instance) {
        log::info(R"(Create drop-copy (user-stream) for account="{}", margin_mode={})"sv, listen_key_update.account, listen_key_update.margin_mode);
        auto &account_2 = get_account(listen_key_update.account);
        auto &request = request_[listen_key_update.account];
        instance = std::make_unique<DropCopyPortfolio>(
            *this, context_, ++stream_id_, account_2, shared_, request, listen_key_update.listen_key, listen_key_update.margin_mode);
        MessageInfo message_info;
        Start start;
        create_event_and_dispatch(*instance, message_info, start);
      }
      break;
    }
  }
}

void Gateway::operator()(Rest::SymbolsUpdate &symbols_update) {
  auto [size, start_from] = shared_.symbols(symbols_update.symbols);
  ensure_symbol_slices(size);
  for (auto &item : market_data_1_) {
    (*item).subscribe(start_from);
  }
  for (auto &item : market_data_2_) {
    (*item).subscribe(start_from);
  }
}

// utilities

void Gateway::ensure_symbol_slices(size_t size) {
  auto helper = [&](auto &container, auto priority) {
    auto stream_id = ++stream_id_;
    auto index = std::size(container);
    log::info("Create MarketData (stream_id={}, priority={}, index={})"sv, stream_id, priority, index);
    auto market_data = std::make_unique<MarketData>(*this, context_, stream_id, priority, shared_, index);
    MessageInfo message_info;
    Start start;
    create_event_and_dispatch(*market_data, message_info, start);
    container.emplace_back(std::move(market_data));
  };
  while (std::size(market_data_1_) < size) {
    helper(market_data_1_, Priority::PRIMARY);
  }
  if (!shared_.settings.ws.enable_secondary) {
    return;  // note
  }
  while (std::size(market_data_2_) < size) {
    helper(market_data_2_, Priority::SECONDARY);
  }
}

template <typename... Args>
void Gateway::dispatch(Args &&...args) {
  dispatch_helper(*this, std::forward<Args>(args)...);
}

template <typename... Args>
void Gateway::dispatch_helper(auto &self, Args &&...args) {
  auto helper = [&](auto &target) { target(args...); };
  helper(self.rest_);
  for (auto &item : self.market_data_1_) {
    helper(*item);
  }
  for (auto &item : self.market_data_2_) {
    helper(*item);
  }
  for (auto &[_, item] : self.order_entry_) {
    helper(*item);
  }
  for (auto &[_, item] : self.drop_copy_) {
    helper(*item);
  }
  for (auto &[_, item] : self.order_entry_margin_) {
    helper(*item);
  }
  for (auto &[_, item] : self.drop_copy_margin_) {
    helper(*item);
  }
}

Account &Gateway::get_account(std::string_view const &account) {
  auto iter = accounts_.find(account);
  if (iter == std::end(accounts_)) [[unlikely]] {
    throw RuntimeError{R"(Unknown account="{}")"sv, account};
  }
  return *(*iter).second;
}

OrderEntry &Gateway::get_order_entry(std::string_view const &account, MarginMode margin_mode) {
  auto helper = [&](auto &lookup) -> OrderEntry & {
    auto iter = lookup.find(account);
    if (iter == std::end(lookup)) [[unlikely]] {
      throw RuntimeError{R"(Unknown account="{}")"sv, account};
    }
    return *(*iter).second;
  };
  switch (margin_mode) {
    using enum MarginMode;
    case UNDEFINED:
      break;
    case ISOLATED:
    case CROSS:
      return helper(order_entry_margin_);
    case PORTFOLIO:
      break;
  }
  return helper(order_entry_);
}

}  // namespace binance
}  // namespace roq
