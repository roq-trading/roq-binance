/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/gateway.h"

#include <cctype>
#include <limits>

#include "roq/logging.h"
#include "roq/format.h"

#include "roq/core/charconv.h"
#include "roq/core/clock.h"
#include "roq/core/utils.h"
#include "roq/core/view.h"

#include "roq/binance/options.h"

#include "roq/binance/json/utils.h"

namespace roq {
namespace binance {

constexpr auto DEFAULT_MULTIPLIER = double{1.0};

constexpr auto TOLERANCE = double{1.0e-10};

constexpr auto DEFAULT_LISTEN_KEY_REFRESH_SECS = uint32_t{2};

template <typename T>
static bool mbp_update(
    auto& data,
    size_t& offset,
    const T& item) {
  auto& obj = data[offset];
  new (&obj) MBPUpdate {
    .price = item.price,
    .quantity = item.qty,
  };
  ++offset;
  return offset < data.size();
}

Gateway::Gateway(
    server::Dispatcher& dispatcher,
    const Config& config)
    : _dispatcher(dispatcher),
      _account(config.get_account()),
      _random(
          config.get_api_key(),
          config.get_secret()),
      _dns_base(_base, true),
      _rest {
        .connection = {
          *this,
          config,
          _random,
          _base,
          _dns_base,
          _ssl_context,
        },
        .download = RestDownload(
            std::chrono::seconds { FLAGS_download_timeout_secs },
            [this](auto state) {
              return download(state);
            }),
      },
      _bid(FLAGS_cache_mbp_max_depth),
      _ask(FLAGS_cache_mbp_max_depth) {
  LOG_IF(WARNING, FLAGS_cancel_on_disconnect == false)(
      "Orders will *NOT* be cancelled on disconnect");
}

void Gateway::operator()(const StartEvent& event) {
  LOG(INFO)("Starting the gateway...");
  _rest.connection(event);
  assert(_symbols.empty());
  assert(_market_streams.empty());
  assert(static_cast<bool>(_user_stream) == false);
  _rest.download.begin();
}

void Gateway::operator()(const StopEvent& event) {
  LOG(INFO)("Stopping the gateway...");
  _rest.connection(event);
  for (auto& iter : _market_streams)
    (*iter)(event);
  if (static_cast<bool>(_user_stream))
    (*_user_stream)(event);
}

void Gateway::operator()(const TimerEvent& event) {
  _rest.connection(event);
  for (auto& iter : _market_streams)
    (*iter)(event);
  if (static_cast<bool>(_user_stream))
    (*_user_stream)(event);
  refresh_listen_key();
  _base.loop(EVLOOP_NONBLOCK);
}

void Gateway::operator()(const ConnectionStatusEvent&) {
}

void Gateway::operator()(
    const CreateOrderEvent& event,
    const std::string_view& request_id,
    uint32_t gateway_order_id) {
}

void Gateway::operator()(
    const ModifyOrderEvent&,
    const std::string_view&,
    const server::OMS_Order& order) {
  throw server::OMS_Exception(
      Error::MODIFY_ORDER_NOT_SUPPORTED,
      order);
}

void Gateway::operator()(
    const CancelOrderEvent& event,
    const std::string_view& request_id,
    const server::OMS_Order& order) {
}

void Gateway::operator()(Metrics& metrics) {
  _rest.connection(metrics);
  for (auto& iter : _market_streams)
    (*iter)(metrics);
  if (static_cast<bool>(_user_stream))
    (*_user_stream)(metrics);
}

// market stream

void Gateway::operator()(const json::AggTrade& agg_trade) {
  Trade trade {
    .side = agg_trade.buyer_is_maker ? Side::BUY : Side::SELL,
    .price = agg_trade.price,
    .quantity = agg_trade.quantity,
    .trade_id = {},
  };
  core::view_t view(
      trade.trade_id,
      trade.trade_id + sizeof(trade.trade_id));
  core::charconv::to_string(
      std::back_inserter(view),
      agg_trade.agg_trade_id);
  TradeSummary trade_summary {
    .exchange = FLAGS_exchange,
    .symbol = agg_trade.symbol,
    .trades = {
      .items = &trade,
      .length = 1,
    },
    .exchange_time_utc = agg_trade.event_time,
  };
  VLOG(3)(
      FMT_STRING(R"(trade_summary={})"),
      trade_summary);
  enqueue(
      trade_summary,
      false);
}

void Gateway::operator()(const json::Trade& trade) {
  Trade trade_ {
    .side = trade.buyer_is_maker ? Side::BUY : Side::SELL,
    .price = trade.price,
    .quantity = trade.quantity,
    .trade_id = {},
  };
  core::view_t view(
      trade_.trade_id,
      trade_.trade_id + sizeof(trade_.trade_id));
  core::charconv::to_string(
      std::back_inserter(view),
      trade.trade_id);
  TradeSummary trade_summary {
    .exchange = FLAGS_exchange,
    .symbol = trade.symbol,
    .trades = {
      .items = &trade_,
      .length = 1,
    },
    .exchange_time_utc = trade.event_time,
  };
  VLOG(3)(
      FMT_STRING(R"(trade_summary={})"),
      trade_summary);
  enqueue(
      trade_summary,
      false);
}

void Gateway::operator()(const json::MiniTicker& mini_ticker) {
  SessionStatistics session_statistics {
    .exchange = FLAGS_exchange,
    .symbol = mini_ticker.symbol,
    .pre_open_interest = std::numeric_limits<double>::quiet_NaN(),
    .pre_settlement_price = std::numeric_limits<double>::quiet_NaN(),
    .pre_close_price = std::numeric_limits<double>::quiet_NaN(),
    .highest_traded_price = mini_ticker.high_price,
    .lowest_traded_price = mini_ticker.low_price,
    .upper_limit_price = std::numeric_limits<double>::quiet_NaN(),
    .lower_limit_price = std::numeric_limits<double>::quiet_NaN(),
    .index_value = std::numeric_limits<double>::quiet_NaN(),
    .margin_rate = std::numeric_limits<double>::quiet_NaN(),
    .exchange_time_utc = mini_ticker.event_time,
  };
  VLOG(3)(
      FMT_STRING(R"(session_statistics={})"),
      session_statistics);
  enqueue(
      session_statistics,
      false);
  DailyStatistics daily_statistics {
    .exchange = FLAGS_exchange,
    .symbol = mini_ticker.symbol,
    .open_price = mini_ticker.open_price,
    .settlement_price = std::numeric_limits<double>::quiet_NaN(),
    .close_price = mini_ticker.close_price,
    .open_interest = std::numeric_limits<double>::quiet_NaN(),
    .exchange_time_utc = mini_ticker.event_time,
  };
  VLOG(3)(
      FMT_STRING(R"(daily_statistics={})"),
      daily_statistics);
  enqueue(
      daily_statistics,
      true);
}

void Gateway::operator()(const json::BookTicker& book_ticker) {
  TopOfBook top_of_book {
    .exchange = FLAGS_exchange,
    .symbol = book_ticker.symbol,
    .layer = {
      .bid_price = book_ticker.best_bid_price,
      .bid_quantity = book_ticker.best_bid_qty,
      .ask_price = book_ticker.best_ask_price,
      .ask_quantity = book_ticker.best_ask_qty,
    },
    .snapshot = false,
    .exchange_time_utc = {},
  };
  VLOG(3)(
      FMT_STRING(R"(top_of_book={})"),
      top_of_book);
  enqueue(
      top_of_book,
      true);
}

void Gateway::operator()(
    const std::string_view& symbol,
    const json::Depth& depth) {
  bool success = true;
  size_t bid_length = 0;
  for (auto& item : depth.bids) {
    if (success == false)
      break;
    success = mbp_update(
        _bid,
        bid_length,
        item);
  }
  size_t ask_length = 0;
  for (auto& item : depth.asks) {
    if (success == false)
      break;
    success = mbp_update(
        _ask,
        ask_length,
        item);
  }
  if (unlikely(success == false)) {
    LOG(FATAL)(
        FMT_STRING(
          R"(Insufficient bid/ask array size(s): )"
          R"(len(bid)={}/{}, len(ask)={}/{})"),
        bid_length,
        _bid.size(),
        ask_length,
        _ask.size());
  }
  MarketByPrice market_by_price {
    .exchange = FLAGS_exchange,
    .symbol = symbol,
    .bids = {
      .items = _bid.data(),
      .length = bid_length,
    },
    .asks = {
      .items = _ask.data(),
      .length = ask_length,
    },
    .snapshot = true,
    .exchange_time_utc = {},
    };
  VLOG(3)(
      FMT_STRING(R"(market_by_price={})"),
      market_by_price);
  enqueue(
      market_by_price,
      true);
}

void Gateway::operator()(
    const std::string_view& symbol,
    const json::DepthUpdate&) {
}

void Gateway::operator()(
    const json::OutboundAccountInfo& outbound_account_info) {
  for (auto& item : outbound_account_info.balances) {
    FundsUpdate funds_update {
      .account = _account,
      .currency = item.asset,
      .balance = item.free_amount,
      .hold = item.locked_amount,
    };
    enqueue(
        funds_update,
        true);
  }
}

void Gateway::operator()(
    const json::OutboundAccountPosition& outbound_account_position) {
  for (auto& item : outbound_account_position.balances) {
    FundsUpdate funds_update {
      .account = _account,
      .currency = item.asset,
      .balance = item.free_amount,
      .hold = item.locked_amount,
    };
    enqueue(
        funds_update,
        true);
  }
}

void Gateway::operator()(const json::BalanceUpdate&) {
  // contains delta (changes) -- we're not going to use here
}

void Gateway::operator()(const json::ExecutionReport&) {
}

// rest

void Gateway::operator()(const Rest&) {
  if (_rest.connection.ready())
    _rest.download.bump();
}


// UTILS:

void Gateway::update_market_data(GatewayStatus gateway_status) {
  if (gateway_status == _market_data_status)
    return;
  _market_data_status = gateway_status;
  MarketDataStatus market_data_status {
    .status = _market_data_status,
  };
  enqueue(
      market_data_status,
      true);
  LOG(INFO)(FMT_STRING("market_data_status={}"), _market_data_status);
}

void Gateway::update_order_manager(GatewayStatus gateway_status) {
  if (gateway_status == _order_manager_status)
    return;
  _order_manager_status = gateway_status;
  OrderManagerStatus order_manager_status {
    .account = _account,
    .status = _order_manager_status,
  };
  enqueue(
      order_manager_status,
      true);
  LOG(INFO)(FMT_STRING("order_manager_status={}"), _order_manager_status);
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
  auto sequence = _rest.download.sequence();
  _rest.connection.get<json::ExchangeInfo>(
      [this, sequence](auto& promise) {
    try {
      if (_rest.download.skip(sequence, state))
        return;
      (*this)(promise.get());
      _rest.download.check(state);
    } catch (NetworkError&) {
      _rest.download.retry(state);
    }
  });
}

void Gateway::subscribe_market_streams() {
  if (_symbols.empty())
    return;
  assert(FLAGS_ws_max_subscriptions > 0);
  size_t max_length = FLAGS_ws_max_subscriptions / 4;
  for (size_t major = 0; major < _symbols.size(); major += max_length) {
    auto minor_length = std::min(
        _symbols.size() - major,
        max_length);
    assert(minor_length > 0);
    std::vector<std::string> symbols(minor_length);
    for (size_t minor = 0; minor < minor_length; ++minor)
      symbols[minor] = _symbols[major + minor];
    auto market_stream_ptr = std::make_unique<MarketStream>(
        *this,
        _random,
        _base,
        _dns_base,
        _ssl_context,
        ++_market_stream_id,
        std::move(symbols));
    (*market_stream_ptr)(StartEvent{});
    _market_streams.emplace_back(std::move(market_stream_ptr));
  }
}

void Gateway::download_listen_key() {
  constexpr auto state = RestDownload::State::LISTEN_KEY;
  auto sequence = _rest.download.sequence();
  _rest.connection.get<json::ListenKey>(
      [this, sequence](auto& promise) {
    try {
      if (_rest.download.skip(sequence, state))
        return;
      (*this)(promise.get());
      _rest.download.check(state);
    } catch (NetworkError&) {
      _rest.download.retry(state);
    }
  });
}

void Gateway::subscribe_user_stream() {
  assert(_listen_key.empty() == false);
  assert(static_cast<bool>(_user_stream) == false);
  _user_stream = std::make_unique<UserStream>(
      *this,
      _random,
      _base,
      _dns_base,
      _ssl_context,
      _listen_key);
  (*_user_stream)(StartEvent{});
}

void Gateway::download_account() {
  constexpr auto state = RestDownload::State::ACCOUNT;
  auto sequence = _rest.download.sequence();
  _rest.connection.get<json::Account>(
      [this, sequence](auto& promise) {
    try {
      if (_rest.download.skip(sequence, state))
        return;
      (*this)(promise.get());
      _rest.download.check(state);
    } catch (NetworkError&) {
      _rest.download.retry(state);
    }
  });
}

void Gateway::operator()(const json::ExchangeInfo& exchange_info) {
  assert(_symbols.empty());
  for (const auto& item : exchange_info.symbols) {
    if (_dispatcher.discard_symbol(item.symbol)) {
      VLOG(1)(
          FMT_STRING(R"(Drop symbol="{}")"),
          item.symbol);
      continue;
    }
    // note! convert to lowercase
    std::string symbol(item.symbol);
    std::transform(
        symbol.begin(),
        symbol.end(),
        symbol.begin(),
        [](auto c) { return std::tolower(c); });
    _symbols.push_back(std::move(symbol));
    ReferenceData reference_data {
      .exchange = FLAGS_exchange,
      .symbol = item.symbol,
      .security_type = SecurityType::UNDEFINED,
      .currency = item.base_asset,
      .settlement_currency = item.quote_asset,
      .commission_currency = std::string_view(),
      .tick_size = std::numeric_limits<double>::quiet_NaN(),
      .limit_up = std::numeric_limits<double>::quiet_NaN(),
      .limit_down = std::numeric_limits<double>::quiet_NaN(),
      .multiplier = std::numeric_limits<double>::quiet_NaN(),
      .min_trade_vol = std::numeric_limits<double>::quiet_NaN(),
      .option_type = OptionType::UNDEFINED,
      .strike_currency = std::string_view(),
      .strike_price = std::numeric_limits<double>::quiet_NaN(),
    };
    VLOG(1)(
        FMT_STRING(R"(reference_data={})"),
        reference_data);
    enqueue(
        reference_data,
        false);
    MarketStatus market_status {
      .exchange = FLAGS_exchange,
      .symbol = item.symbol,
      .trading_status = json::map(item.status),
    };
    VLOG(1)(
        FMT_STRING(R"(market_status={})"),
        market_status);
    enqueue(
        market_status,
        true);
  }
  LOG(INFO)(
      FMT_STRING("Exchange info: including symbols {}/{}"),
      _symbols.size(),
      exchange_info.symbols.size());
}

void Gateway::operator()(const json::ListenKey& listen_key) {
  auto& value = listen_key.listen_key;
  auto same = _listen_key.compare(value) == 0;
  if (same) {
    LOG(INFO)("Listen key has been refreshed!");
  } else if (_listen_key.empty()) {
    _listen_key = value;
    LOG(INFO)(
        FMT_STRING(R"(Listen key has been acquired (value="{}"))"),
        _listen_key);
  } else {
    LOG(FATAL)("Unexpected");
    // XXX should we recreate _user_stream ?
  }
  auto now = core::get_system_clock();
  _listen_key_refresh = now
    + std::chrono::seconds { FLAGS_rest_listen_key_refresh_secs };
}

void Gateway::operator()(const json::Account& account) {
  for (auto& item : account.balances) {
    FundsUpdate funds_update {
      .account = _account,
      .currency = item.asset,
      .balance = item.free,
      .hold = item.locked,
    };
    enqueue(
        funds_update,
        true);
  }
}

void Gateway::refresh_listen_key() {
  auto now = core::get_system_clock();
  if (_listen_key_refresh.count() == 0 ||
      now < _listen_key_refresh)
    return;
  LOG(INFO)("Refreshing listen key...");
  _listen_key_refresh = now
    + std::chrono::seconds { FLAGS_rest_listen_key_refresh_secs };
  _rest.connection.get<json::ListenKey>(
      [this](auto& promise) {
    try {
      (*this)(promise.get());
    } catch (NetworkError&) {
      LOG(WARNING)("Rescheduling listen key refresh!");
      auto now = core::get_system_clock();
      _listen_key_refresh = now
        + std::chrono::seconds { DEFAULT_LISTEN_KEY_REFRESH_SECS };
    }
  });
}

template <typename T>
void Gateway::enqueue(
    const T& event,
    bool is_last) {
  auto now = core::get_system_clock();
  _dispatcher(
      event,
      now,
      now,
      is_last);
}

template <typename T>
void Gateway::enqueue(
    uint8_t user_id,
    const T& event,
    bool is_last) {
  auto now = core::get_system_clock();
  _dispatcher(
      user_id,
      event,
      now,
      now,
      is_last);
}

}  // namespace binance
}  // namespace roq
