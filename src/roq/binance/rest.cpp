/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/rest.h"

#include <utility>

#include "roq/utils/mask.h"
#include "roq/utils/update.h"

#include "roq/core/charconv.h"

#include "roq/core/metrics/factory.h"

#include "roq/binance/flags.h"

#include "roq/binance/json/utils.h"

using namespace roq::literals;

namespace roq {
namespace binance {

namespace {
static const auto NAME = "rest"_sv;
static const auto SUPPORTS = utils::Mask{
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
};

static const auto ALLOW_PIPELINING = true;

static const auto X_MBX_USED_WEIGHT = "x-mbx-used-weight-1m"_sv;

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

Rest::Rest(Handler &handler, core::io::Context &context, uint16_t stream_id, Shared &shared)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"_sv, stream_id_, NAME)),
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
          Flags::rest_rate_limit_interval(),
          Flags::rest_rate_limit_max_requests(),
          Flags::rest_ping_freq(),
          Flags::rest_ping_path()),
      decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_metrics(name_, "disconnect"_sv),
      },
      profile_{
          .exchange_info = create_metrics(name_, "exchange_info"_sv),
          .exchange_info_ack = create_metrics(name_, "exchange_info_ack"_sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"_sv),
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
  connection_.refresh(event.value.now);
}

void Rest::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.exchange_info, metrics::PROFILE)
      .write(profile_.exchange_info_ack, metrics::PROFILE)
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
  server::TraceInfo trace_info;
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(trace_info, external_latency, handler_);
  latency_.ping.update(latency.sample);
}

void Rest::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    server::TraceInfo trace_info;
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::REST,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"_sv, stream_status);
    server::create_trace_and_dispatch(trace_info, stream_status, handler_);
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
    auto path = "/api/v3/exchangeInfo"_sv;
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = {},
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
        .rate_limit_weight = 1,
    };
    connection_(
        "exchange_info"_sv, request, [this]([[maybe_unused]] auto &request_id, auto &response) {
          get_exchange_info_ack(response);
        });
  });
}

void Rest::get_exchange_info_ack(const core::web::Response &response) {
  profile_.exchange_info_ack([&]() {
    server::TraceInfo trace_info;
    auto state = RestState::EXCHANGE_INFO;
    try {
      response.expect(core::http::Status::OK);
      auto body = response.body();
      core::json::Buffer buffer(decode_buffer_);
      auto exchange_info = core::json::Parser::create<json::ExchangeInfo>(body, buffer);
      server::Trace event(trace_info, exchange_info);
      (*this)(event);
      download_.check(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"_sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void Rest::operator()(const server::Trace<json::ExchangeInfo> &event) {
  auto &[trace_info, exchange_info] = event;
  std::vector<std::string> symbols;
  size_t counter = {};
  for (const auto &item : exchange_info.symbols) {
    log::info<1>("item={}"_sv, item);
    if (shared_.discard_symbol(item.symbol)) {
      log::info<1>(R"(Drop symbol="{}")"_sv, item.symbol);
      continue;
    }
    // note! convert to lowercase
    std::string symbol(item.symbol);
    std::transform(
        symbol.begin(), symbol.end(), symbol.begin(), [](auto c) { return std::tolower(c); });
    if (all_symbols_.emplace(symbol).second)  // only include new
      symbols.emplace_back(symbol);
    ++counter;
    auto tick_size = std::pow(10.0, -static_cast<double>(item.quote_precision));
    auto min_trade_vol = std::pow(10.0, -static_cast<double>(item.base_asset_precision));
    ReferenceData reference_data{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = item.symbol,
        .description = {},
        .security_type = {},
        .currency = item.quote_asset,
        .settlement_currency = item.base_asset,
        .commission_currency = {},
        .tick_size = tick_size,
        .multiplier = NaN,
        .min_trade_vol = min_trade_vol,
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
    create_trace_and_dispatch(trace_info, reference_data, handler_, false);
    auto trading_status = json::map(item.status);
    MarketStatus market_status{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = item.symbol,
        .trading_status = trading_status,
    };
    create_trace_and_dispatch(trace_info, market_status, handler_, true);
  }
  log::info("Exchange info: including symbols {}/{}"_sv, counter, exchange_info.symbols.size());
  if (!symbols.empty()) {
    SymbolsUpdate symbols_update{
        .symbols = symbols,
    };
    handler_(symbols_update);
  }
}

}  // namespace binance
}  // namespace roq
