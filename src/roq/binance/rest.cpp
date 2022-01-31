/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/rest.h"

#include <algorithm>
#include <utility>

#include "roq/utils/mask.h"
#include "roq/utils/update.h"

#include "roq/core/back_emplacer.h"
#include "roq/core/charconv.h"

#include "roq/core/metrics/factory.h"

#include "roq/binance/flags.h"

#include "roq/binance/json/filters.h"
#include "roq/binance/json/utils.h"

using namespace std::literals;

namespace roq {
namespace binance {

namespace {
const auto NAME = "rest"sv;
const auto SUPPORTS = utils::Mask{
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
};

const auto ALLOW_PIPELINING = true;

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};

template <typename T>
void emplace(MBPUpdate &result, const T &value) {
  new (&result) MBPUpdate{
      .price = value.price,
      .quantity = value.qty,
      .implied_quantity = NaN,
      .price_level = {},
      .number_of_orders = {},
  };
}
}  // namespace

Rest::Rest(Handler &handler, core::io::Context &context, uint16_t stream_id, Shared &shared)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      connection_(
          *this,
          context,
          Flags::decode_buffer_size(),
          Flags::encode_buffer_size(),
          core::URI(Flags::rest_uri()),
          ROQ_PACKAGE_NAME,
          core::http::Connection::KEEP_ALIVE,
          ALLOW_PIPELINING,
          Flags::rest_request_timeout(),
          Flags::rest_ping_freq(),
          Flags::rest_ping_path()),
      decode_buffer_(Flags::decode_buffer_size()), decode_buffer_2_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .exchange_info = create_metrics(name_, "exchange_info"sv),
          .exchange_info_ack = create_metrics(name_, "exchange_info_ack"sv),
          .depth = create_metrics(name_, "depth"sv),
          .depth_ack = create_metrics(name_, "depth_ack"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
      },
      shared_(shared),
      download_(Flags::rest_request_timeout(), [this](auto state) { return download(state); }) {
}

void Rest::operator()(const Event<Start> &) {
  connection_.start();
}

void Rest::operator()(const Event<Stop> &) {
  connection_.stop();
}

void Rest::operator()(const Event<Timer> &event) {
  auto now = event.value.now;
  connection_.refresh(now);
  if (ready())
    check_request_queue(now);
}

void Rest::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.exchange_info, metrics::PROFILE)
      .write(profile_.exchange_info_ack, metrics::PROFILE)
      .write(profile_.depth, metrics::PROFILE)
      .write(profile_.depth_ack, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY);
}

void Rest::operator()(const core::web::Client::Connected &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void Rest::operator()(const core::web::Client::Disconnected &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading())
    download_.reset();
}

void Rest::operator()(const core::web::Client::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void Rest::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::REST,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"sv, stream_status);
    server::create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t Rest::download(RestState state) {
  switch (state) {
    case RestState::UNDEFINED:
      assert(false);
      break;
    case RestState::EXCHANGE_INFO:
      get_exchange_info();
      return 1;
    case RestState::DONE:
      (*this)(ConnectionStatus::READY);
      assert(!ready_);
      ready_ = true;
      return {};
  }
  assert(false);
  return {};
}

// exchange-info

void Rest::get_exchange_info() {
  profile_.exchange_info([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v3/exchangeInfo"sv;
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "exchange_info"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_exchange_info_ack(event, sequence);
        });
  });
}

void Rest::get_exchange_info_ack(
    const server::Trace<core::web::Response> &event, uint32_t sequence) {
  profile_.exchange_info_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = RestState::EXCHANGE_INFO;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto exchange_info = core::json::Parser::create<json::ExchangeInfo>(body, buffer);
      server::Trace event(trace_info, exchange_info);
      (*this)(event);
      download_.check(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void Rest::operator()(const server::Trace<json::ExchangeInfo> &event) {
  auto &[trace_info, exchange_info] = event;
  log::info<2>("exchange_info={}"sv, exchange_info);
  std::vector<std::string> symbols;
  size_t counter = {};
  for (const auto &item : exchange_info.symbols) {
    log::info<2>("item={}"sv, item);
    if (shared_.discard_symbol(item.symbol)) {
      log::info<1>(R"(Drop symbol="{}")"sv, item.symbol);
      continue;
    }
    log::debug("item={}"sv, item);
    // fall-back values
    auto tick_size = std::pow(10.0, -static_cast<double>(item.quote_precision));
    auto min_trade_vol = std::pow(10.0, -static_cast<double>(item.base_asset_precision));
    auto max_trade_vol = NaN;
    auto trade_vol_step_size = min_trade_vol;
    // parse filters and update
    core::json::Buffer buffer(decode_buffer_2_);
    auto filters = core::json::Parser::create<json::Filters>(item.filters, buffer);
    for (auto &filter : filters.data) {
      switch (filter.filter_type) {
        case json::FilterType::UNDEFINED:
          break;
        case json::FilterType::UNKNOWN:
          break;
        case json::FilterType::PRICE_FILTER:
          tick_size = filter.tick_size;
          break;
        case json::FilterType::PERCENT_PRICE:
          break;
        case json::FilterType::LOT_SIZE:
          min_trade_vol = filter.min_qty;
          max_trade_vol = filter.max_qty;
          trade_vol_step_size = filter.step_size;
          break;
        case json::FilterType::MIN_NOTIONAL:
          break;
        case json::FilterType::ICEBERG_PARTS:
          break;
        case json::FilterType::MARKET_LOT_SIZE:
          break;
        case json::FilterType::MAX_NUM_ORDERS:
          break;
        case json::FilterType::MAX_NUM_ALGO_ORDERS:
          break;
        case json::FilterType::MAX_NUM_ICEBERG_ORDERS:
          break;
        case json::FilterType::MAX_POSITION:
          break;
        case json::FilterType::EXCHANGE_MAX_NUM_ORDERS:
          break;
        case json::FilterType::EXCHANGE_MAX_NUM_ALGO_ORDERS:
          break;
      }
    }
    // note! convert to lowercase
    std::string symbol(item.symbol);
    std::transform(std::begin(symbol), std::end(symbol), std::begin(symbol), [](auto c) {
      return std::tolower(c);
    });
    if (all_symbols_.emplace(symbol).second)  // only include new
      symbols.emplace_back(symbol);
    ++counter;
    ReferenceData reference_data{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = item.symbol,
        .description = {},
        .security_type = {},
        .base_currency = item.base_asset,
        .quote_currency = item.quote_asset,
        .margin_currency = {},
        .commission_currency = {},
        .tick_size = tick_size,
        .multiplier = NaN,
        .min_trade_vol = min_trade_vol,
        .max_trade_vol = max_trade_vol,
        .trade_vol_step_size = trade_vol_step_size,
        .option_type = {},
        .strike_currency = {},
        .strike_price = NaN,
        .underlying = {},
        .time_zone = {},
        .issue_date = {},
        .settlement_date = {},
        .expiry_datetime = {},
        .expiry_datetime_utc = {},
    };
    create_trace_and_dispatch(handler_, trace_info, reference_data, false);
    auto trading_status = json::map(item.status);
    MarketStatus market_status{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = item.symbol,
        .trading_status = trading_status,
    };
    create_trace_and_dispatch(handler_, trace_info, market_status, true);
  }
  log::info("Exchange info: including symbols {}/{}"sv, counter, std::size(exchange_info.symbols));
  if (!std::empty(symbols)) {
    SymbolsUpdate symbols_update{
        .symbols = symbols,
    };
    handler_(symbols_update);
  }
}

// depth

void Rest::get_depth(const std::string_view &symbol) {
  profile_.depth([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v3/depth"sv;
    auto query = fmt::format("?symbol={}&limit={}"sv, symbol, Flags::ws_subscribe_depth_levels());
    core::web::Request request{
        .method = method,
        .path = path,
        .query = query,
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    connection_(
        "depth"sv,
        request,
        [this, symbol = std::string{symbol}]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_depth_ack(event, symbol);
        });
  });
}

void Rest::get_depth_ack(
    const server::Trace<core::web::Response> &event, const std::string_view &symbol) {
  profile_.depth_ack([&]() {
    auto &[trace_info, response] = event;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto depth = core::json::Parser::create<json::Depth>(body, buffer);
      server::Trace event(trace_info, depth);
      (*this)(event, symbol);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      // XXX HANS ???
    }
  });
}

void Rest::operator()(const server::Trace<json::Depth> &event, const std::string_view &symbol) {
  // auto &[trace_info, depth] = event;
  auto &trace_info = event.trace_info;
  auto &depth = event.value;
  log::info<4>(R"(depth={}, symbol="{}")"sv, depth, symbol);
  auto sequence = depth.last_update_id;
  auto &collector = shared_.mbp_collector[symbol];
  core::back_emplacer bids(shared_.bids), asks(shared_.asks);
  for (auto &item : depth.bids)
    bids.emplace_back([&item](auto &result) { emplace(result, item); });
  for (auto &item : depth.asks)
    asks.emplace_back([&item](auto &result) { emplace(result, item); });
  try {
    collector(
        bids,
        asks,
        sequence,
        [&](auto &bids, auto &asks, auto sequence) {  // snapshot
          log::debug(R"(PUBLISH SNAPSHOT symbol="{}", sequence={})"sv, symbol, sequence);
          MarketByPriceUpdate market_by_price_update{
              .stream_id = stream_id_,
              .exchange = Flags::exchange(),
              .symbol = symbol,
              .bids = bids,
              .asks = asks,
              .update_type = UpdateType::SNAPSHOT,
              .exchange_time_utc = {},
              .exchange_sequence = collector.last_sequence(),
              .price_decimals = {},
              .quantity_decimals = {},
              .checksum = {},
          };
          server::Trace event(trace_info, market_by_price_update);
          shared_(event, true, [&](auto &market_by_price) {
            collector.apply(market_by_price, sequence, false);
          });
        },
        [&](auto retries) {  // request
          log::debug(R"(REQUEST symbol="{}" (retries={}))"sv, symbol, retries);
          if (Flags::ws_mbp_request_max_retries() &&
              Flags::ws_mbp_request_max_retries() < retries) {
            log::fatal(R"(Unexpected: symbol="{}", retries={})"sv, symbol, retries);
          }
          shared_.depth_request_queue.emplace_back(symbol);
        });
  } catch (BadState &) {
    log::warn(R"(RESUBSCRIBE symbol="{}")"sv, symbol);
    // XXX HANS publish stale
    collector.clear();
    shared_.depth_request_queue.emplace_back(symbol);
  }
}

// queue

void Rest::check_request_queue(std::chrono::nanoseconds now) {
  shared_.depth_request_queue.dispatch(
      [&](auto now) { return shared_.rate_limiter.can_request(now); },
      [&](auto &symbol) {
        log::debug(R"(Requesting order book snapshot symbol="{}")"sv, symbol);
        get_depth(symbol);
      },
      now);
}

}  // namespace binance
}  // namespace roq
