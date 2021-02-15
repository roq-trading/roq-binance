/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/gateway.h"

#include <algorithm>
#include <cctype>
#include <limits>

#include "roq/logging.h"

#include "roq/core/string_utils/fixed_string_builder.h"

#include "roq/core/charconv.h"
#include "roq/core/clock.h"
#include "roq/core/utils.h"
#include "roq/core/view.h"

#include "roq/binance/flags.h"

#include "roq/binance/json/utils.h"

using namespace roq::literals;

namespace roq {
namespace binance {

constexpr auto DEFAULT_MULTIPLIER = double{1.0};

constexpr auto TOLERANCE = double{1.0e-10};

constexpr auto DEFAULT_LISTEN_KEY_REFRESH_SECS = uint32_t{2};

template <typename C, typename T>
static bool mbp_update(C &data, size_t &offset, const T &item) {
  if (offset >= data.size())
    return false;
  auto &obj = data[offset];
  new (&obj) MBPUpdate{
      .price = item.price,
      .quantity = item.qty,
  };
  ++offset;
  return offset <= data.size();
}

Gateway::Gateway(server::Dispatcher &dispatcher, const Config &config)
    : dispatcher_(dispatcher), account_(config.get_account()),
      random_(config.get_api_key(), config.get_secret()), dns_base_(base_, true),
      rest_{
          .connection =
              {
                  *this,
                  config,
                  random_,
                  base_,
                  dns_base_,
                  ssl_context_,
              },
          .download = RestDownload(
              std::chrono::seconds{Flags::rest_request_timeout_secs()},
              [this](auto state) { return download(state); }),
      },
      bid_(Flags::cache_mbp_max_depth()), ask_(Flags::cache_mbp_max_depth()) {
  LOG_IF(FATAL, Flags::rest_cancel_on_disconnect())
  ("Exchange does *NOT* support cancel on disconnect"_sv);
}

void Gateway::operator()(const Event<Start> &event) {
  LOG(INFO)("Starting the gateway..."_sv);
  rest_.connection(event);
  assert(symbols_.empty());
  assert(market_streams_.empty());
  assert(static_cast<bool>(user_stream_) == false);
  rest_.download.begin();
}

void Gateway::operator()(const Event<Stop> &event) {
  LOG(INFO)("Stopping the gateway..."_sv);
  rest_.connection(event);
  for (auto &iter : market_streams_)
    (*iter)(event);
  if (static_cast<bool>(user_stream_))
    (*user_stream_)(event);
}

void Gateway::operator()(const Event<Timer> &event) {
  rest_.connection(event);
  for (auto &iter : market_streams_)
    (*iter)(event);
  if (static_cast<bool>(user_stream_))
    (*user_stream_)(event);
  refresh_listen_key();
  base_.loop(EVLOOP_NONBLOCK);
}

void Gateway::operator()(const Event<Connection> &) {
}

void Gateway::operator()(
    const Event<CreateOrder> &event,
    const std::string_view &request_id,
    [[maybe_unused]] uint32_t gateway_order_id) {
  rest_.connection.create_order(event.value, request_id, [this](auto &promise) {
    try {
      (*this)(promise.get());
    } catch (NetworkError &e) {
      // XXX send ack failure
      LOG(FATAL)(R"(Unexpected what="{}")"_sv, e.what());
    }
  });
}

void Gateway::operator()(
    const Event<ModifyOrder> &, const std::string_view &, const server::OMS_Order &order) {
  throw server::OMS_Exception(Error::MODIFY_ORDER_NOT_SUPPORTED, order);
}

void Gateway::operator()(
    const Event<CancelOrder> &event,
    const std::string_view &request_id,
    const server::OMS_Order &order) {
  rest_.connection.cancel_order(event.value, request_id, order, [this](auto &promise) {
    try {
      (*this)(promise.get());
    } catch (NetworkError &e) {
      // XXX send ack failure
      LOG(FATAL)(R"(Unexpected what="{}")"_sv, e.what());
    }
  });
}

void Gateway::operator()(metrics::Writer &writer) {
  rest_.connection(writer);
  for (auto &iter : market_streams_)
    (*iter)(writer);
  if (static_cast<bool>(user_stream_))
    (*user_stream_)(writer);
}

// all

void Gateway::operator()(
    const ExternalLatency &external_latency, const server::TraceInfo &trace_info) {
  create_trace_and_dispatch(trace_info, external_latency, dispatcher_);
}

// market stream

void Gateway::operator()(const json::AggTrade &agg_trade, const server::TraceInfo &trace_info) {
  Trade trade{
      .side = agg_trade.buyer_is_maker ? Side::BUY : Side::SELL,
      .price = agg_trade.price,
      .quantity = agg_trade.quantity,
      .trade_id = {},
  };
  core::charconv::to_string(
      core::string_utils::fixed_string_builder(trade.trade_id), agg_trade.agg_trade_id);
  TradeSummary trade_summary{
      .exchange = Flags::exchange(),
      .symbol = agg_trade.symbol,
      .trades = {&trade, 1},
      .exchange_time_utc = agg_trade.event_time,
  };
  VLOG(3)(R"(trade_summary={})"_sv, trade_summary);
  create_trace_and_dispatch(trace_info, trade_summary, dispatcher_, true);
}

void Gateway::operator()(const json::Trade &trade, const server::TraceInfo &trace_info) {
  Trade trade_{
      .side = trade.buyer_is_maker ? Side::BUY : Side::SELL,
      .price = trade.price,
      .quantity = trade.quantity,
      .trade_id = {},
  };
  core::charconv::to_string(
      core::string_utils::fixed_string_builder(trade_.trade_id), trade.trade_id);
  TradeSummary trade_summary{
      .exchange = Flags::exchange(),
      .symbol = trade.symbol,
      .trades = {&trade_, 1},
      .exchange_time_utc = trade.event_time,
  };
  VLOG(3)(R"(trade_summary={})"_sv, trade_summary);
  create_trace_and_dispatch(trace_info, trade_summary, dispatcher_, true);
}

void Gateway::operator()(const json::MiniTicker &mini_ticker, const server::TraceInfo &trace_info) {
  Statistics statistics[] = {
      {
          .type = StatisticsType::HIGHEST_TRADED_PRICE,
          .value = mini_ticker.high_price,
      },
      {
          .type = StatisticsType::LOWEST_TRADED_PRICE,
          .value = mini_ticker.low_price,
      },
      {
          .type = StatisticsType::OPEN_PRICE,
          .value = mini_ticker.open_price,
      },
      {
          .type = StatisticsType::CLOSE_PRICE,
          .value = mini_ticker.close_price,
      },
  };
  static_assert(std::size(statistics) == 4);  // just checking...
  StatisticsUpdate statistics_update{
      .exchange = Flags::exchange(),
      .symbol = mini_ticker.symbol,
      .statistics = roq::span(statistics, std::size(statistics)),
      .snapshot = false,
      .exchange_time_utc = mini_ticker.event_time,
  };
  VLOG(3)("statistics_update={}"_sv, statistics_update);
  create_trace_and_dispatch(trace_info, statistics_update, dispatcher_, true);
}

void Gateway::operator()(const json::BookTicker &book_ticker, const server::TraceInfo &trace_info) {
  TopOfBook top_of_book{
      .exchange = Flags::exchange(),
      .symbol = book_ticker.symbol,
      .layer =
          {
              .bid_price = book_ticker.best_bid_price,
              .bid_quantity = book_ticker.best_bid_qty,
              .ask_price = book_ticker.best_ask_price,
              .ask_quantity = book_ticker.best_ask_qty,
          },
      .snapshot = false,
      .exchange_time_utc = {},
  };
  VLOG(3)(R"(top_of_book={})"_sv, top_of_book);
  create_trace_and_dispatch(trace_info, top_of_book, dispatcher_, true);
}

void Gateway::operator()(
    const std::string_view &symbol, const json::Depth &depth, const server::TraceInfo &trace_info) {
  bool success = true;
  size_t bid_length = 0;
  for (auto &item : depth.bids) {
    if (success == false)
      break;
    success = mbp_update(bid_, bid_length, item);
  }
  size_t ask_length = 0;
  for (auto &item : depth.asks) {
    if (success == false)
      break;
    success = mbp_update(ask_, ask_length, item);
  }
  LOG_IF(WARNING, !success)
  (R"(Insufficient bid/ask array size(s): )"
   R"(symbol="{}", len(bid)={}/{}, len(ask)={}/{})"_sv,
   symbol,
   depth.bids.size(),
   bid_.size(),
   depth.asks.size(),
   ask_.size());
  MarketByPriceUpdate market_by_price_update{
      .exchange = Flags::exchange(),
      .symbol = symbol,
      .bids = {bid_.data(), bid_length},
      .asks = {ask_.data(), ask_length},
      .snapshot = true,
      .exchange_time_utc = {},
  };
  VLOG(3)(R"(market_by_price_update={})"_sv, market_by_price_update);
  create_trace_and_dispatch(trace_info, market_by_price_update, dispatcher_, true);
}

void Gateway::operator()(
    const std::string_view &, const json::DepthUpdate &, const server::TraceInfo &) {
}

void Gateway::operator()(
    const json::OutboundAccountInfo &outbound_account_info, const server::TraceInfo &trace_info) {
  for (auto &item : outbound_account_info.balances) {
    FundsUpdate funds_update{
        .account = account_,
        .currency = item.asset,
        .balance = item.free_amount,
        .hold = item.locked_amount,
        .external_account = {},
    };
    create_trace_and_dispatch(trace_info, funds_update, dispatcher_, true);
  }
}

void Gateway::operator()(
    const json::OutboundAccountPosition &outbound_account_position,
    const server::TraceInfo &trace_info) {
  for (auto &item : outbound_account_position.balances) {
    FundsUpdate funds_update{
        .account = account_,
        .currency = item.asset,
        .balance = item.free_amount,
        .hold = item.locked_amount,
        .external_account = {},
    };
    create_trace_and_dispatch(trace_info, funds_update, dispatcher_, true);
  }
}

void Gateway::operator()(const json::BalanceUpdate &, const server::TraceInfo &) {
  // contains delta (changes) -- we're not going to use here
}

void Gateway::operator()(
    const json::ExecutionReport &execution_report, const server::TraceInfo &trace_info) {
  server::OMS_Lookup order_lookup{
      .symbol = execution_report.symbol,
      .side = json::map(execution_report.side),
      .status = json::map(execution_report.current_order_status),
      .price = execution_report.price,
      .remaining_quantity = std::numeric_limits<double>::quiet_NaN(),
      .traded_quantity = execution_report.cumulative_filled_quantity,
      .timestamp = execution_report.transaction_time,  // XXX transact_time?
      .external_account = {},
      .external_order_id = execution_report.client_order_id,
  };
  auto found = dispatcher_.find_order(
      execution_report.original_client_order_id,
      execution_report.client_order_id,
      order_lookup,
      trace_info,
      [&]([[maybe_unused]] const auto &order, [[maybe_unused]] auto &result) {
        // XXX IMPLEMENT
      });
  if (found == false) {
    LOG(WARNING)("*** EXTERNAL ORDER ***"_sv);
    LOG(WARNING)("execution_report={}"_sv, execution_report);
  }
}

// rest

void Gateway::operator()(const Rest &) {
  if (rest_.connection.ready())
    rest_.download.bump();
}

void Gateway::operator()(const json::NewOrder &) {
}

void Gateway::operator()(const json::CancelOrder &) {
}

// UTILS:

void Gateway::update_market_data(GatewayStatus gateway_status) {
  if (gateway_status == market_data_status_)
    return;
  market_data_status_ = gateway_status;
  server::TraceInfo trace_info;
  MarketDataStatus market_data_status{
      .status = market_data_status_,
  };
  create_trace_and_dispatch(trace_info, market_data_status, dispatcher_, true);
  LOG(INFO)("market_data_status={}"_sv, market_data_status_);
}

void Gateway::update_order_manager(GatewayStatus gateway_status) {
  if (gateway_status == order_manager_status_)
    return;
  order_manager_status_ = gateway_status;
  server::TraceInfo trace_info;
  OrderManagerStatus order_manager_status{
      .account = account_,
      .status = order_manager_status_,
  };
  create_trace_and_dispatch(trace_info, order_manager_status, dispatcher_, true);
  LOG(INFO)("order_manager_status={}"_sv, order_manager_status_);
}

uint32_t Gateway::download(RestDownload::State state) {
  switch (state) {
    case RestDownload::State::UNDEFINED:
      assert(false);
      break;
    case RestDownload::State::EXCHANGE_INFO:
      download_exchange_info();
      return 1;
    case RestDownload::State::MARKET_STREAM:
      subscribe_market_streams();
      return 0;
    case RestDownload::State::LISTEN_KEY:
      download_listen_key();
      return 1;
    case RestDownload::State::USER_STREAM:
      subscribe_user_stream();
      return 0;
    case RestDownload::State::ACCOUNT:
      download_account();
      return 1;
    case RestDownload::State::DONE:
      // update(GatewayStatus::READY);
      return 0;
  }
  assert(false);
  return 0;
}

void Gateway::download_exchange_info() {
  constexpr auto state = RestDownload::State::EXCHANGE_INFO;
  auto sequence = rest_.download.sequence();
  rest_.connection.get<json::ExchangeInfo>([this, sequence](auto &promise) {
    try {
      if (rest_.download.skip(sequence, state))
        return;
      (*this)(promise.get());
      rest_.download.check(state);
    } catch (NetworkError &) {
      rest_.download.retry(state);
    }
  });
}

void Gateway::subscribe_market_streams() {
  if (symbols_.empty())
    return;
  assert(Flags::ws_max_subscriptions_per_stream() > 0);
  size_t max_length = Flags::ws_max_subscriptions_per_stream() / 4;
  auto iter = symbols_.begin();
  for (size_t major = 0; major < symbols_.size(); major += max_length) {
    auto minor_length = std::min(symbols_.size() - major, max_length);
    assert(minor_length > 0);
    std::vector<std::string> symbols;
    symbols.reserve(max_length);
    for (size_t minor = 0; minor < minor_length; ++minor, ++iter) {
      assert(iter != symbols_.end());
      symbols.emplace_back(*iter);
    }
    auto market_stream_ptr = std::make_unique<MarketStream>(
        *this, random_, base_, dns_base_, ssl_context_, ++market_stream_id_, std::move(symbols));
    MessageInfo message_info;  // XXX something sensible
    Start start;
    create_event_and_dispatch(*market_stream_ptr, message_info, start);
    market_streams_.emplace_back(std::move(market_stream_ptr));
  }
}

void Gateway::download_listen_key() {
  constexpr auto state = RestDownload::State::LISTEN_KEY;
  auto sequence = rest_.download.sequence();
  rest_.connection.get<json::ListenKey>([this, sequence](auto &promise) {
    try {
      if (rest_.download.skip(sequence, state))
        return;
      (*this)(promise.get());
      rest_.download.check(state);
    } catch (NetworkError &) {
      rest_.download.retry(state);
    }
  });
}

void Gateway::subscribe_user_stream() {
  assert(listen_key_.empty() == false);
  assert(static_cast<bool>(user_stream_) == false);
  user_stream_ =
      std::make_unique<UserStream>(*this, random_, base_, dns_base_, ssl_context_, listen_key_);
  MessageInfo message_info;  // XXX something sensible
  Start start;
  create_event_and_dispatch(*user_stream_, message_info, start);
}

void Gateway::download_account() {
  constexpr auto state = RestDownload::State::ACCOUNT;
  auto sequence = rest_.download.sequence();
  rest_.connection.get<json::Account>([this, sequence](auto &promise) {
    try {
      if (rest_.download.skip(sequence, state))
        return;
      (*this)(promise.get());
      rest_.download.check(state);
    } catch (NetworkError &) {
      rest_.download.retry(state);
    }
  });
}

void Gateway::operator()(const json::ExchangeInfo &exchange_info) {
  assert(symbols_.empty());
  server::TraceInfo trace_info;  // note! not correct (*after* message parsing)
  for (const auto &item : exchange_info.symbols) {
    VLOG(1)(R"(item={})"_sv, item);
    if (dispatcher_.discard_symbol(item.symbol)) {
      VLOG(1)(R"(Drop symbol="{}")"_sv, item.symbol);
      continue;
    }
    // note! convert to lowercase
    std::string symbol(item.symbol);
    std::transform(
        symbol.begin(), symbol.end(), symbol.begin(), [](auto c) { return std::tolower(c); });
    symbols_.emplace(std::move(symbol));
    auto tick_size = std::pow(10.0, -static_cast<double>(item.quote_precision));
    auto min_trade_vol = std::pow(10.0, -static_cast<double>(item.base_asset_precision));
    ReferenceData reference_data{
        .exchange = Flags::exchange(),
        .symbol = item.symbol,
        .description = {},
        .security_type = SecurityType::UNDEFINED,
        .currency = item.quote_asset,
        .settlement_currency = item.base_asset,
        .commission_currency = {},
        .tick_size = tick_size,
        .multiplier = std::numeric_limits<double>::quiet_NaN(),
        .min_trade_vol = min_trade_vol,
        .option_type = OptionType::UNDEFINED,
        .strike_currency = {},
        .strike_price = std::numeric_limits<double>::quiet_NaN(),
        .underlying = {},
        .time_zone = {},
        .issue_date = {},
        .settlement_date = {},
        .expiry_datetime = {},
        .expiry_datetime_utc = {},
    };
    VLOG(1)(R"(reference_data={})"_sv, reference_data);
    create_trace_and_dispatch(trace_info, reference_data, dispatcher_, false);
    MarketStatus market_status{
        .exchange = Flags::exchange(),
        .symbol = item.symbol,
        .trading_status = json::map(item.status),
    };
    VLOG(1)(R"(market_status={})"_sv, market_status);
    create_trace_and_dispatch(trace_info, market_status, dispatcher_, true);
  }
  LOG(INFO)
  ("Exchange info: including symbols {}/{}"_sv, symbols_.size(), exchange_info.symbols.size());
}

void Gateway::operator()(const json::ListenKey &listen_key) {
  auto &value = listen_key.listen_key;
  auto same = listen_key_.compare(value) == 0;
  if (same) {
    LOG(INFO)("Listen key has been refreshed!"_sv);
  } else if (listen_key_.empty()) {
    listen_key_ = value;
    LOG(INFO)(R"(Listen key has been acquired (value="{}"))"_sv, listen_key_);
  } else {
    LOG(FATAL)("Unexpected"_sv);
    // XXX should we recreate user_stream_ ?
  }
  auto now = core::get_system_clock();
  listen_key_refresh_ = now + std::chrono::seconds{Flags::rest_listen_key_refresh_secs()};
}

void Gateway::operator()(const json::Account &account) {
  server::TraceInfo trace_info;  // note! not correct (*after* message parsing)
  for (auto &item : account.balances) {
    FundsUpdate funds_update{
        .account = account_,
        .currency = item.asset,
        .balance = item.free,
        .hold = item.locked,
        .external_account = {},
    };
    create_trace_and_dispatch(trace_info, funds_update, dispatcher_, true);
  }
}

void Gateway::refresh_listen_key() {
  auto now = core::get_system_clock();
  if (listen_key_refresh_ == listen_key_refresh_.zero() || now < listen_key_refresh_)
    return;
  LOG(INFO)("Refreshing listen key..."_sv);
  listen_key_refresh_ = now + std::chrono::seconds{Flags::rest_listen_key_refresh_secs()};
  rest_.connection.get<json::ListenKey>([this](auto &promise) {
    try {
      (*this)(promise.get());
    } catch (NetworkError &) {
      LOG(WARNING)("Rescheduling listen key refresh!"_sv);
      auto now = core::get_system_clock();
      listen_key_refresh_ = now + std::chrono::seconds{DEFAULT_LISTEN_KEY_REFRESH_SECS};
    }
  });
}

/*
template <typename T>
void Gateway::enqueue(
    const T& event,
    const server::Trace& trace,
    bool is_last) {
  dispatcher_(
      event,
      trace,
      is_last);
}

template <typename T>
void Gateway::enqueue(
    uint8_t user_id,
    const T& event,
    const server::Trace& trace,
    bool is_last) {
  dispatcher_(
      user_id,
      event,
      trace,
      is_last);
}
*/

}  // namespace binance
}  // namespace roq
