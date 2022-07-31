/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/rest.hpp"

#include <algorithm>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/update.hpp"

#include "roq/core/back_emplacer.hpp"
#include "roq/core/charconv.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/rest/client_factory.hpp"

#include "roq/binance/flags.hpp"

#include "roq/binance/json/filters.hpp"
#include "roq/binance/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

namespace {
auto const NAME = "rest"sv;

const Mask SUPPORTS{
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
};

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(std::string_view const &group, std::string_view const &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};

template <typename T>
void emplace(MBPUpdate &result, const T &value) {
  new (&result) MBPUpdate{
      .price = value.price,
      .quantity = value.qty,
      .implied_quantity = NaN,
      .number_of_orders = {},
      .update_action = {},
      .price_level = {},
  };
}

auto create_connection(auto &handler, auto &context) {
  auto uri = Flags::rest_uri();
  web::rest::Client::Config config{
      .decode_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
      .validate_certificate = server::Flags::net_tls_validate_certificate(),
      .uris = {&uri, 1},
      .proxy = Flags::rest_proxy(),
      .user_agent = ROQ_PACKAGE_NAME,
      .connection = web::http::Connection::KEEP_ALIVE,
      .allow_pipelining = true,
      .request_timeout = Flags::rest_request_timeout(),
      .ping_frequency = Flags::rest_ping_freq(),
      .ping_path = Flags::rest_ping_path(),
  };
  return web::rest::ClientFactory::create(handler, context, config);
}
}  // namespace

Rest::Rest(Handler &handler, io::Context &context, uint16_t stream_id, Shared &shared)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      connection_(create_connection(*this, context)), decode_buffer_(Flags::decode_buffer_size()),
      decode_buffer_2_(Flags::decode_buffer_size()),
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
      shared_(shared), download_(Flags::rest_request_timeout(), [this](auto state) { return download(state); }) {
}

void Rest::operator()(Event<Start> const &) {
  (*connection_).start();
}

void Rest::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void Rest::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
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

void Rest::operator()(web::rest::Client::Connected const &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void Rest::operator()(web::rest::Client::Disconnected const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading())
    download_.reset();
}

void Rest::operator()(web::rest::Client::Latency const &latency) {
  auto trace_info = server::create_trace_info();
  const ExternalLatency external_latency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void Rest::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    const StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::HTTP,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t Rest::download(RestState state) {
  switch (state) {
    using enum RestState;
    case UNDEFINED:
      assert(false);
      break;
    case EXCHANGE_INFO:
      get_exchange_info();
      return 1;
    case DONE:
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
    auto method = web::http::Method::GET;
    auto path = "/api/v3/exchangeInfo"sv;
    web::rest::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    (*connection_)("exchange_info"sv, request, [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
      auto trace_info = server::create_trace_info();
      Trace event(trace_info, response);
      get_exchange_info_ack(event, sequence);
    });
  });
}

void Rest::get_exchange_info_ack(Trace<web::rest::Response const> const &event, uint32_t sequence) {
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
      response.expect(web::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      const auto exchange_info = core::json::Parser::create<json::ExchangeInfo>(body, buffer);
      Trace event(trace_info, exchange_info);
      (*this)(event);
      download_.check(state);
    } catch (NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void Rest::operator()(Trace<json::ExchangeInfo const> const &event) {
  auto &[trace_info, exchange_info] = event;
  // log::info<2>("exchange_info={}"sv, exchange_info);
  std::vector<Symbol> symbols;
  size_t counter = {};
  for (auto const &item : exchange_info.symbols) {
    log::info<2>("item={}"sv, item);
    auto discard = shared_.discard_symbol(item.symbol);
    // fall-back values
    auto tick_size = std::pow(10.0, -static_cast<double>(item.quote_precision));
    auto min_trade_vol = std::pow(10.0, -static_cast<double>(item.base_asset_precision));
    auto max_trade_vol = NaN;
    auto trade_vol_step_size = min_trade_vol;
    auto min_notional = NaN;
    // parse filters and update
    core::json::Buffer buffer(decode_buffer_2_);
    auto filters = core::json::Parser::create<json::Filters>(item.filters, buffer);
    for (auto &filter : filters.data) {
      switch (filter.filter_type) {
        using enum json::FilterType::type_t;
        case UNDEFINED:
          break;
        case UNKNOWN:
          break;
        case PRICE_FILTER:
          tick_size = filter.tick_size;
          break;
        case PERCENT_PRICE:
          break;
        case LOT_SIZE:
          min_trade_vol = filter.min_qty;
          max_trade_vol = filter.max_qty;
          trade_vol_step_size = filter.step_size;
          break;
        case MIN_NOTIONAL:
          min_notional = filter.min_notional;
          break;
        case ICEBERG_PARTS:
          break;
        case MARKET_LOT_SIZE:
          break;
        case MAX_NUM_ORDERS:
          break;
        case MAX_NUM_ALGO_ORDERS:
          break;
        case MAX_NUM_ICEBERG_ORDERS:
          break;
        case MAX_POSITION:
          break;
        case EXCHANGE_MAX_NUM_ORDERS:
          break;
        case EXCHANGE_MAX_NUM_ALGO_ORDERS:
          break;
        case TRAILING_DELTA:
          break;
      }
    }
    const ReferenceData reference_data{
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
        .min_notional = min_notional,
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
        .discard = discard,
    };
    create_trace_and_dispatch(handler_, trace_info, reference_data, false);
    if (discard) {
      log::info<1>(R"(Drop symbol="{}")"sv, item.symbol);
      continue;
    }
    // note! convert to lowercase
    std::string symbol(item.symbol);
    std::transform(std::begin(symbol), std::end(symbol), std::begin(symbol), [](auto c) { return std::tolower(c); });
    if (all_symbols_.emplace(symbol).second)  // only include new
      symbols.emplace_back(symbol);
    ++counter;
    auto trading_status = json::map(item.status);
    const MarketStatus market_status{
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

void Rest::get_depth(std::string_view const &symbol) {
  profile_.depth([&]() {
    auto method = web::http::Method::GET;
    auto path = "/api/v3/depth"sv;
    auto query = fmt::format("?symbol={}&limit={}"sv, symbol, Flags::ws_subscribe_depth_levels());
    web::rest::Request request{
        .method = method,
        .path = path,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    (*connection_)(
        "depth"sv, request, [this, symbol = std::string{symbol}]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          Trace event(trace_info, response);
          get_depth_ack(event, symbol);
        });
  });
}

void Rest::get_depth_ack(Trace<web::rest::Response const> const &event, std::string_view const &symbol) {
  profile_.depth_ack([&]() {
    auto &[trace_info, response] = event;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      response.expect(web::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      const auto depth = core::json::Parser::create<json::Depth>(body, buffer);
      Trace event(trace_info, depth);
      (*this)(event, symbol);
    } catch (NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      // XXX HANS ???
    }
  });
}

void Rest::operator()(Trace<json::Depth const> const &event, std::string_view const &symbol) {
  // auto &[trace_info, depth] = event; // XXX clang13
  auto &trace_info = event.trace_info;
  auto &depth = event.value;
  log::info<4>(R"(depth={}, symbol="{}")"sv, depth, symbol);
  auto sequence = depth.last_update_id;
  core::back_emplacer bids(shared_.bids), asks(shared_.asks);
  for (auto &item : depth.bids)
    bids.emplace_back([&item](auto &result) { emplace(result, item); });
  for (auto &item : depth.asks)
    asks.emplace_back([&item](auto &result) { emplace(result, item); });
  auto &collector = shared_.mbp_collector[symbol];
  try {
    collector(
        bids,
        asks,
        sequence,
        [&](auto &bids, auto &asks, auto sequence) {  // snapshot
          log::debug(R"(PUBLISH SNAPSHOT symbol="{}", sequence={})"sv, symbol, sequence);
          const MarketByPriceUpdate market_by_price_update{
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
          Trace event(trace_info, market_by_price_update);
          shared_(event, true, [&](auto &market_by_price) { collector.apply(market_by_price, sequence, false); });
        },
        [&](auto retries) {  // request
          log::debug(R"(REQUEST symbol="{}" (retries={}))"sv, symbol, retries);
          if (Flags::ws_mbp_request_max_retries() && Flags::ws_mbp_request_max_retries() < retries) {
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
