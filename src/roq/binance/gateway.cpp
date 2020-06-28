/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/gateway.h"

#include <cctype>
#include <limits>

#include "roq/logging.h"
#include "roq/format.h"

#include "roq/core/string/builder.h"

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

void Gateway::operator()(const Event<Start>& event) {
  LOG(INFO)("Starting the gateway...");
  _rest.connection(event);
  assert(_symbols.empty());
  assert(_market_streams.empty());
  assert(static_cast<bool>(_user_stream) == false);
  _rest.download.begin();
}

void Gateway::operator()(const Event<Stop>& event) {
  LOG(INFO)("Stopping the gateway...");
  _rest.connection(event);
  for (auto& iter : _market_streams)
    (*iter)(event);
  if (static_cast<bool>(_user_stream))
    (*_user_stream)(event);
}

void Gateway::operator()(const Event<Timer>& event) {
  _rest.connection(event);
  for (auto& iter : _market_streams)
    (*iter)(event);
  if (static_cast<bool>(_user_stream))
    (*_user_stream)(event);
  refresh_listen_key();
  _base.loop(EVLOOP_NONBLOCK);
}

void Gateway::operator()(const Event<Connection>&) {
}

void Gateway::operator()(
    const Event<CreateOrder>& event,
    const std::string_view& request_id,
    uint32_t gateway_order_id) {
  (void) gateway_order_id;  // avoid warning
  _rest.connection.create_order(
      event.value,
      request_id,
      [this](auto& promise) {
    try {
      (*this)(promise.get());
    } catch (NetworkError& e) {
      // XXX send ack failure
      LOG(FATAL)(
          FMT_STRING(R"(Unexpected what="{}")"),
          e.what());
    }
  });
}

void Gateway::operator()(
    const Event<ModifyOrder>&,
    const std::string_view&,
    const server::OMS_Order& order) {
  throw server::OMS_Exception(
      Error::MODIFY_ORDER_NOT_SUPPORTED,
      order);
}

void Gateway::operator()(
    const Event<CancelOrder>& event,
    const std::string_view& request_id,
    const server::OMS_Order& order) {
  _rest.connection.cancel_order(
      event.value,
      request_id,
      order,
      [this](auto& promise) {
    try {
      (*this)(promise.get());
    } catch (NetworkError& e) {
      // XXX send ack failure
      LOG(FATAL)(
          FMT_STRING(R"(Unexpected what="{}")"),
          e.what());
    }
  });
}

void Gateway::operator()(metrics::Writer& writer) {
  _rest.connection(writer);
  for (auto& iter : _market_streams)
    (*iter)(writer);
  if (static_cast<bool>(_user_stream))
    (*_user_stream)(writer);
}

// market stream

void Gateway::operator()(
    const json::AggTrade& agg_trade,
    const server::Trace& trace) {
  Trade trade {
    .side = agg_trade.buyer_is_maker ? Side::BUY : Side::SELL,
    .price = agg_trade.price,
    .quantity = agg_trade.quantity,
    .trade_id = {},
  };
  core::charconv::to_string(
      core::string::builder(trade.trade_id),
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
      trace,
      false);
}

void Gateway::operator()(
    const json::Trade& trade,
    const server::Trace& trace) {
  Trade trade_ {
    .side = trade.buyer_is_maker ? Side::BUY : Side::SELL,
    .price = trade.price,
    .quantity = trade.quantity,
    .trade_id = {},
  };
  core::charconv::to_string(
      core::string::builder(trade_.trade_id),
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
      trace,
      false);  // XXX (2020-06-21) why ???
}

void Gateway::operator()(
    const json::MiniTicker& mini_ticker,
    const server::Trace& trace) {
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
      trace,
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
      trace,
      true);
  // XXX NEW
  Statistics statistics[] = {
    {
      .type = StatisticsType::HIGHEST_TRADED_PRICE,
      .value = mini_ticker.high_price,
    }, {
      .type = StatisticsType::LOWEST_TRADED_PRICE,
      .value = mini_ticker.low_price,
    }, {
      .type = StatisticsType::OPEN_PRICE,
      .value = mini_ticker.open_price,
    }, {
      .type = StatisticsType::CLOSE_PRICE,
      .value = mini_ticker.close_price,
    }, };
  static_assert(std::size(statistics) == 4);  // just checking...
  StatisticsUpdate statistics_update {
    .exchange = FLAGS_exchange,
    .symbol = mini_ticker.symbol,
    .statistics = roq::span(
        statistics,
        std::size(statistics)),
    .snapshot = false,
    .exchange_time_utc = mini_ticker.event_time,
  };
  VLOG(3)(
      FMT_STRING("statistics_update={}"),
      statistics_update);
  enqueue(
      statistics_update,
      trace,
      true);
}

void Gateway::operator()(
    const json::BookTicker& book_ticker,
    const server::Trace& trace) {
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
      trace,
      true);
}

void Gateway::operator()(
    const std::string_view& symbol,
    const json::Depth& depth,
    const server::Trace& trace) {
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
  if (ROQ_PREDICT_FALSE(success == false)) {
    LOG(FATAL)(
        FMT_STRING(
          R"(Insufficient bid/ask array size(s): )"
          R"(len(bid)={}/{}, len(ask)={}/{})"),
        bid_length,
        _bid.size(),
        ask_length,
        _ask.size());
  }
  MarketByPriceUpdate market_by_price_update {
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
      FMT_STRING(R"(market_by_price_update={})"),
      market_by_price_update);
  enqueue(
      market_by_price_update,
      trace,
      true);
}

void Gateway::operator()(
    const std::string_view&,
    const json::DepthUpdate&,
    const server::Trace&) {
}

void Gateway::operator()(
    const json::OutboundAccountInfo& outbound_account_info,
    const server::Trace& trace) {
  for (auto& item : outbound_account_info.balances) {
    FundsUpdate funds_update {
      .account = _account,
      .currency = item.asset,
      .balance = item.free_amount,
      .hold = item.locked_amount,
    };
    enqueue(
        funds_update,
        trace,
        true);
  }
}

void Gateway::operator()(
    const json::OutboundAccountPosition& outbound_account_position,
    const server::Trace& trace) {
  for (auto& item : outbound_account_position.balances) {
    FundsUpdate funds_update {
      .account = _account,
      .currency = item.asset,
      .balance = item.free_amount,
      .hold = item.locked_amount,
    };
    enqueue(
        funds_update,
        trace,
        true);
  }
}

void Gateway::operator()(
    const json::BalanceUpdate&,
    const server::Trace&) {
  // contains delta (changes) -- we're not going to use here
}

void Gateway::operator()(
    const json::ExecutionReport& execution_report,
    const server::Trace& trace) {
  server::OMS_Lookup order_lookup {
    .symbol = execution_report.symbol,
    .side = json::map(execution_report.side),
    .status = json::map(execution_report.current_order_status),
    .price = execution_report.price,
    .remaining_quantity = std::numeric_limits<double>::quiet_NaN(),
    .traded_quantity = execution_report.cumulative_filled_quantity,
    .timestamp = execution_report.transaction_time,  // XXX transact_time?
    .external_order_id = execution_report.client_order_id,
  };
  auto found = _dispatcher.find_order(
      execution_report.original_client_order_id,
      execution_report.client_order_id,
      order_lookup,
      trace,
      [&](const auto& order, auto& result) {
        (void)(order);
        (void)(result);
        // XXX IMPLEMENT
      });
  if (found == false) {
    LOG(WARNING)("*** EXTERNAL ORDER ***");
    LOG(WARNING)(
        FMT_STRING("execution_report={}"),
        execution_report);
  }
}

// rest

void Gateway::operator()(const Rest&) {
  if (_rest.connection.ready())
    _rest.download.bump();
}

void Gateway::operator()(const json::NewOrder&) {
}

void Gateway::operator()(const json::CancelOrder&) {
}

// UTILS:

void Gateway::update_market_data(GatewayStatus gateway_status) {
  if (gateway_status == _market_data_status)
    return;
  _market_data_status = gateway_status;
  server::Trace trace;
  MarketDataStatus market_data_status {
    .status = _market_data_status,
  };
  enqueue(
      market_data_status,
      trace,
      true);
  LOG(INFO)(FMT_STRING("market_data_status={}"), _market_data_status);
}

void Gateway::update_order_manager(GatewayStatus gateway_status) {
  if (gateway_status == _order_manager_status)
    return;
  _order_manager_status = gateway_status;
  server::Trace trace;
  OrderManagerStatus order_manager_status {
    .account = _account,
    .status = _order_manager_status,
  };
  enqueue(
      order_manager_status,
      trace,
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
  auto iter = _symbols.begin();
  for (size_t major = 0; major < _symbols.size(); major += max_length) {
    auto minor_length = std::min(
        _symbols.size() - major,
        max_length);
    assert(minor_length > 0);
    std::vector<std::string> symbols;
    symbols.reserve(max_length);
    for (size_t minor = 0; minor < minor_length; ++minor, ++iter) {
      assert(iter != _symbols.end());
      symbols.emplace_back(*iter);
    }
    auto market_stream_ptr = std::make_unique<MarketStream>(
        *this,
        _random,
        _base,
        _dns_base,
        _ssl_context,
        ++_market_stream_id,
        std::move(symbols));
    MessageInfo message_info;  // XXX something sensible
    Start start;
    create_event_and_dispatch(
        *market_stream_ptr,
        message_info,
        start);
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
  MessageInfo message_info; // XXX something sensible;
  Start start;
  create_event_and_dispatch(
      *_user_stream,
      message_info,
      start);
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
  server::Trace trace;  // note! not correct (*after* message parsing)
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
    _symbols.emplace(std::move(symbol));
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
        trace,
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
        trace,
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
  server::Trace trace;  // note! not correct (*after* message parsing)
  for (auto& item : account.balances) {
    FundsUpdate funds_update {
      .account = _account,
      .currency = item.asset,
      .balance = item.free,
      .hold = item.locked,
    };
    enqueue(
        funds_update,
        trace,
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
    const server::Trace& trace,
    bool is_last) {
  _dispatcher(
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
  _dispatcher(
      user_id,
      event,
      trace,
      is_last);
}

}  // namespace binance
}  // namespace roq
