/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/rest.hpp"

#include <algorithm>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/update.hpp"

#include "roq/utils/charconv.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/binance/utils.hpp"

#include "roq/binance/json/error.hpp"
#include "roq/binance/json/filters.hpp"
#include "roq/binance/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const NAME = "rest"sv;

auto const SUPPORTS = Mask{
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
};
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id) {
  return fmt::format("{}:{}"sv, stream_id, NAME);
}

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = settings.rest.uri;
  auto config = web::rest::Client::Config{
      // connection
      .interface = {},
      .proxy = settings.rest.proxy,
      .uris = {&uri, 1},
      .host = settings.rest.host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = {},
      .disconnect_on_idle_timeout = {},
      .connection = web::http::Connection::KEEP_ALIVE,
      // request
      .allow_pipelining = true,
      .request_timeout = settings.rest.request_timeout,
      // response
      .suspend_on_retry_after = true,
      // http
      .query = {},
      .user_agent = ROQ_PACKAGE_NAME,
      .ping_frequency = settings.rest.ping_freq,
      .ping_path = settings.rest.ping_path,
      // implementation
      .decode_buffer_size = settings.common.decode_buffer_size,
      .encode_buffer_size = settings.common.encode_buffer_size,
  };
  return web::rest::Client::create(handler, context, config);
}

struct create_metrics final : public core::metrics::Factory {
  create_metrics(auto &settings, auto const &group, auto const &function)
      : core::metrics::Factory(settings.app.name, group, function) {}
  create_metrics(auto &settings, auto const &group, auto const &function, auto const &params)
      : core::metrics::Factory(settings.app.name, group, function, params) {}
};
}  // namespace

// === IMPLEMENTATION ===

Rest::Rest(Handler &handler, io::Context &context, uint16_t stream_id, Shared &shared)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_)},
      connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_(shared.settings.common.decode_buffer_size),
      decode_buffer_2_(shared.settings.common.decode_buffer_size),
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .exchange_info = create_metrics(shared.settings, name_, "exchange_info"sv),
          .exchange_info_ack = create_metrics(shared.settings, name_, "exchange_info_ack"sv),
          .depth = create_metrics(shared.settings, name_, "depth"sv),
          .depth_ack = create_metrics(shared.settings, name_, "depth_ack"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
      },
      rate_limiter_{
          .request_weight_1m = create_metrics(shared.settings, name_, "request_weight"sv, "1m"sv),
      },
      shared_{shared}, download_{shared.settings.rest.request_timeout, [this](auto state) { return download(state); }} {
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
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.exchange_info, metrics::Type::PROFILE)
      .write(profile_.exchange_info_ack, metrics::Type::PROFILE)
      .write(profile_.depth, metrics::Type::PROFILE)
      .write(profile_.depth_ack, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      // rate limiter
      .write(rate_limiter_.request_weight_1m, metrics::Type::RATE_LIMITER);
}

void Rest::operator()(Trace<web::rest::Client::Connected> const &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void Rest::operator()(Trace<web::rest::Client::Disconnected> const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading())
    download_.reset();
}

void Rest::operator()(Trace<web::rest::Client::Latency> const &event) {
  auto &[trace_info, latency] = event;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void Rest::operator()(Trace<web::rest::Client::MessageBegin> const &) {
  shared_.rate_limits.clear();
}

void Rest::operator()(Trace<web::rest::Client::Header> const &event) {
  auto &header = event.value;
  if (header.name == "x-mbx-used-weight-1m"sv) {
    try {
      auto value = utils::from_string_relaxed<uint32_t>(header.value);
      auto rate_limit = RateLimit{
          .type = RateLimitType::REQUEST_WEIGHT,
          .period = 1min,
          .end_time_utc = {},
          .limit = shared_.limits.request_weight_1m,
          .value = value,
      };
      shared_.rate_limits.emplace_back(std::move(rate_limit));
      rate_limiter_.request_weight_1m.set(value);
    } catch (RuntimeError &) {
      log::warn<5>(R"(Failed to parse text="{}")"sv, header.value);
    }
  }
}

void Rest::operator()(Trace<web::rest::Client::MessageEnd> const &event) {
  auto &trace_info = event.trace_info;
  if (std::empty(shared_.rate_limits))
    return;
  auto rate_limits_update = RateLimitsUpdate{
      .stream_id = stream_id_,
      .account = {},
      .origin = Origin::EXCHANGE,
      .rate_limits = shared_.rate_limits,
  };
  create_trace_and_dispatch(handler_, trace_info, rate_limits_update);
  shared_.rate_limits.clear();
}

void Rest::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    auto stream_status = StreamStatus{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::HTTP,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
        .interface = (*connection_).get_interface(),
        .authority = (*connection_).get_current_authority(),
        .path = (*connection_).get_current_path(),
        .proxy = (*connection_).get_proxy(),
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
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = "/api/v3/exchangeInfo"sv,
        .query = {},
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this, sequence = download_.sequence()]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_exchange_info_ack(event, sequence);
    };
    (*connection_)("exchange_info"sv, request, callback);
  });
}

void Rest::get_exchange_info_ack(Trace<web::rest::Response> const &event, uint32_t sequence) {
  constexpr auto const STATE = RestState::EXCHANGE_INFO;
  profile_.exchange_info_ack([&]() {
    auto handle_success = [&](auto &body) {
      if (download_.skip(sequence, STATE)) {
        log::info("Download state={} has already been processed"sv, STATE);
      } else {
        json::ExchangeInfo exchange_info{body, decode_buffer_};
        // log::debug("exchange_info={}"sv, exchange_info);
        Trace event_2{event, exchange_info};
        (*this)(event_2);
        download_.check(STATE);
      }
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      if (download_.downloading())
        download_.retry(STATE);
    };
    process_response(event, handle_success, handle_error);
  });
}

void Rest::operator()(Trace<json::ExchangeInfo> const &event) {
  auto &[trace_info, exchange_info] = event;
  // rate-limits
  for (auto &item : exchange_info.rate_limits) {
    switch (item.rate_limit_type) {
      using enum json::RateLimitType::type_t;
      case UNDEFINED__:
      case UNKNOWN__:
        break;
      case ORDERS:
        switch (item.interval) {
          using enum json::Interval::type_t;
          case UNDEFINED__:
          case UNKNOWN__:
            break;
          case SECOND:
            if (item.interval_num == 10) {
              shared_.limits.create_order_10s = item.limit;
            } else {
              log::warn("Unknown interval: rate_limit={}"sv, item);
            }
            break;
          case MINUTE:
            log::warn("Unknown interval: rate_limit={}"sv, item);
            break;
          case DAY:
            if (item.interval_num == 1) {
              shared_.limits.create_order_1d = item.limit;
            } else {
              log::warn("Unknown interval: rate_limit={}"sv, item);
            }
            break;
        }
        break;
      case REQUEST_WEIGHT:
        shared_.limits.request_weight_1m = item.limit;
        break;
        switch (item.interval) {
          using enum json::Interval::type_t;
          case UNDEFINED__:
          case UNKNOWN__:
            break;
          case SECOND:
            log::warn("Unknown interval: rate_limit={}"sv, item);
            break;
          case MINUTE:
            if (item.interval_num == 1) {
              shared_.limits.request_weight_1m = item.limit;
            } else {
              log::warn("Unknown interval: rate_limit={}"sv, item);
            }
            break;
          case DAY:
            log::warn("Unknown interval: rate_limit={}"sv, item);
            break;
        }
        break;
      case RAW_REQUESTS:
        break;
    }
  }
  // symbols
  std::vector<Symbol> symbols;
  size_t counter = {};
  for (auto const &item : exchange_info.symbols) {
    log::info<2>("item={}"sv, item);
    auto discard = shared_.discard_symbol(item.symbol);  // XXX should this be normalized symbol ???
    // fall-back values
    auto tick_size = std::pow(10.0, -static_cast<double>(item.quote_precision));
    auto min_trade_vol = std::pow(10.0, -static_cast<double>(item.base_asset_precision));
    auto max_trade_vol = NaN;
    auto trade_vol_step_size = min_trade_vol;
    auto min_notional = NaN;
    // parse filters and update
    json::Filters filters{item.filters, decode_buffer_2_};
    for (auto &filter : filters.data) {
      switch (filter.filter_type) {
        using enum json::FilterType::type_t;
        case UNDEFINED__:
          break;
        case UNKNOWN__:
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
        case PERCENT_PRICE_BY_SIDE:
          break;
        case NOTIONAL:
          break;
      }
    }
    auto reference_data = ReferenceData{
        .stream_id = stream_id_,
        .exchange = shared_.settings.exchange,
        .symbol = item.symbol,
        .description = {},
        .security_type = SecurityType::SPOT,
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
        .exchange_time_utc = {},
        .exchange_sequence = {},
        .sending_time_utc = exchange_info.server_time,
        .discard = discard,
    };
    create_trace_and_dispatch(handler_, trace_info, reference_data, false);
    if (discard) {
      log::info<1>(R"(Drop symbol="{}")"sv, item.symbol);
      continue;
    }
    auto symbol = normalized_symbol(item.symbol);
    if (all_symbols_.emplace(symbol).second)  // only include new
      symbols.emplace_back(symbol);
    ++counter;
    auto trading_status = json::map(item.status);
    auto market_status = MarketStatus{
        .stream_id = stream_id_,
        .exchange = shared_.settings.exchange,
        .symbol = item.symbol,
        .trading_status = trading_status,
        .exchange_time_utc = {},
        .exchange_sequence = {},
        .sending_time_utc = exchange_info.server_time,
    };
    create_trace_and_dispatch(handler_, trace_info, market_status, true);
  }
  log::info("Exchange info: including symbols {}/{}"sv, counter, std::size(exchange_info.symbols));
  if (!std::empty(symbols)) {
    auto symbols_update = SymbolsUpdate{
        .symbols = symbols,
    };
    handler_(symbols_update);
  }
}

// depth

void Rest::get_depth(std::string_view const &symbol) {
  profile_.depth([&]() {
    auto query = fmt::format("?symbol={}&limit={}"sv, symbol, shared_.settings.ws.subscribe_depth_levels);
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = "/api/v3/depth"sv,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this, symbol = std::string{symbol}]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_depth_ack(event, symbol);
    };
    (*connection_)("depth"sv, request, callback);
  });
}

void Rest::get_depth_ack(Trace<web::rest::Response> const &event, std::string_view const &symbol) {
  profile_.depth_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::Depth depth{body, decode_buffer_};
      Trace event_2{event, depth};
      (*this)(event_2, symbol);
    };
    auto handle_error = []([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      // XXX WHAT ???
    };
    process_response(event, handle_success, handle_error);
  });
}

void Rest::operator()(Trace<json::Depth> const &event, std::string_view const &symbol) {
  auto &trace_info = event.trace_info;
  auto &depth = event.value;
  log::info<4>(R"(depth={}, symbol="{}")"sv, depth, symbol);
  auto sequence = depth.last_update_id;
  auto &mbp = shared_.get_mbp();
  auto emplace_back = [](auto &result, auto &value) {
    auto mbp_update = MBPUpdate{
        .price = value.price,
        .quantity = value.qty,
        .implied_quantity = NaN,
        .number_of_orders = {},
        .update_action = {},
        .price_level = {},
    };
    result.emplace_back(std::move(mbp_update));
  };
  for (auto &item : depth.bids)
    emplace_back(mbp.bids, item);
  for (auto &item : depth.asks)
    emplace_back(mbp.asks, item);
  auto &instrument = shared_.get_instrument(symbol);
  auto &sequencer = instrument.sequencer;
  try {
    auto publish_snapshot = [&](auto &bids, auto &asks, auto sequence, auto retries, auto delay) {
      log::info(
          R"(DEBUG PUBLISH SNAPSHOT symbol="{}", sequence={}, retries={}, delay={})"sv,
          symbol,
          sequence,
          retries,
          std::chrono::duration_cast<std::chrono::milliseconds>(delay));
      auto market_by_price_update = MarketByPriceUpdate{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = symbol,
          .bids = bids,
          .asks = asks,
          .update_type = UpdateType::SNAPSHOT,
          .exchange_time_utc = {},
          .exchange_sequence = sequencer.last_sequence(),
          .sending_time_utc = {},
          .price_precision = {},
          .quantity_precision = {},
          .checksum = {},
      };
      auto apply_updates = [&](auto &market_by_price) { sequencer.apply(market_by_price, sequence, false); };
      Trace event{trace_info, market_by_price_update};
      shared_(event, true, apply_updates);
    };
    auto request_snapshot = [&](auto retries) {
      log::info(R"(DEBUG REQUEST SNAPSHOT symbol="{}", retries={})"sv, symbol, retries);
      if (shared_.settings.ws.mbp_request_max_retries && shared_.settings.ws.mbp_request_max_retries < retries) {
        log::fatal(R"(Unexpected: symbol="{}", retries={})"sv, symbol, retries);
      }
      shared_.depth_request_queue.emplace_back(symbol);
    };
    sequencer(mbp.bids, mbp.asks, sequence, false, publish_snapshot, request_snapshot);
  } catch (BadState &) {
    log::warn(R"(RESUBSCRIBE symbol="{}")"sv, symbol);
    // XXX FIXME publish stale
    sequencer.clear();
    shared_.depth_request_queue.emplace_back(symbol);
  }
}

void Rest::check_request_queue(std::chrono::nanoseconds now) {
  auto can_request = [&](auto now) { return shared_.rate_limiter.can_request(now); };
  auto request = [&](auto &symbol) {
    log::debug(R"(Requesting order book snapshot symbol="{}")"sv, symbol);
    get_depth(symbol);
  };
  shared_.depth_request_queue.dispatch(can_request, request, now);
}

namespace {
auto get_retry_after(auto &response) {
  std::chrono::nanoseconds result = {};
  response.dispatch(web::http::Header::RETRY_AFTER, [&](auto &value) {
    try {
      // XXX FIXME could also be a datetime (see https://datatracker.ietf.org/doc/html/rfc7231)
      auto seconds = utils::from_string_relaxed<int64_t>(value);
      result = std::chrono::seconds{seconds};
    } catch (RuntimeError &) {
      log::warn<5>(R"(Failed to parse text="{}")"sv, value);
    }
  });
  return result;
}
}  // namespace

template <typename SuccessHandler, typename ErrorHandler>
void Rest::process_response(
    web::rest::Response const &response, SuccessHandler success_handler, ErrorHandler error_handler) {
  try {
    auto [status, category, body] = response.result();
    // log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
    switch (category) {
      using enum web::http::Category;
      case SUCCESS:  // 2xx
        success_handler(body);
        break;
      case CLIENT_ERROR:  // 4xx
        switch (status) {
          using enum web::http::Status;
          case FORBIDDEN:           // 403
            waf_limit_violation();  // note! this is *very* serious
            [[fallthrough]];
          case I_AM_A_TEAPOT:        // 418
          case TOO_MANY_REQUESTS: {  // 429
            auto retry_after = get_retry_after(response);
            if (retry_after.count())
              (*connection_).suspend(retry_after);
            auto text = fmt::format("{}"sv, status);
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::REQUEST_RATE_LIMIT_REACHED, text);
            break;
          }
          case CONFLICT:  // 409
            assert(false);
            [[fallthrough]];
          default: {
            json::Error error{body};
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, json::guess_error(error.code), error.msg);
          }
        }
        break;
      case SERVER_ERROR: {  // 5xx
        auto text = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, text);
        break;
      }
      default:
        response.expect(web::http::Status::OK);  // throws
    }
  } catch (NetworkError &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::GATEWAY, e.request_status(), e.error(), e.what());
  } catch (std::exception &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, e.what());
  }
}

void Rest::test(web::http::Status status) {
  if (status != web::http::Status::FORBIDDEN) [[likely]]
    return;
  waf_limit_violation();
}

void Rest::waf_limit_violation() {
  if (shared_.settings.rest.terminate_on_403) {
    log::fatal("WAF limit violation"sv);
  } else {
    log::warn("WAF limit violation"sv);
    (*connection_).suspend(shared_.settings.rest.back_off_delay);
  }
}

}  // namespace binance
}  // namespace roq
