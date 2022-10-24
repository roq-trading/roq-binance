/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/market_data.hpp"

#include <algorithm>

#include "roq/mask.hpp"

#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/core/back_emplacer.hpp"
#include "roq/core/charconv.hpp"

#include "roq/core/tools/exception.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/socket/client_factory.hpp"

#include "roq/binance/flags.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const NAME = "md"sv;

auto const SUPPORTS = Mask{
    SupportType::TOP_OF_BOOK,
    SupportType::MARKET_BY_PRICE,
    SupportType::TRADE_SUMMARY,
    SupportType::STATISTICS,
};
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id) {
  return fmt::format("{}:{}"sv, stream_id, NAME);
}

auto create_connection(auto &handler, auto &context) {
  auto uri = Flags::ws_uri();
  web::socket::Client::Config config{
      .always_reconnect = true,
      .connection_timeout = server::Flags::net_connection_timeout(),
      .disconnect_on_idle_timeout = server::Flags::net_disconnect_on_idle_timeout(),
      .validate_certificate = server::Flags::net_tls_validate_certificate(),
      .uris = {&uri, 1},
      .query = {},
      .ping_frequency = Flags::ws_ping_freq(),
      .read_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
  };
  return web::socket::ClientFactory::create(handler, context, config, []() { return std::string(); });
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(auto const &group, auto const &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

// === IMPLEMENTATION ===

MarketData::MarketData(Handler &handler, io::Context &context, uint16_t stream_id, Shared &shared, size_t index)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_)}, index_{index},
      connection_{create_connection(*this, context)}, decode_buffer_{Flags::decode_buffer_size()},
      request_id_{static_cast<uint64_t>(stream_id_) * 1000000},  // scale (debugging)
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
          .total_bytes_received = create_metrics(name_, "total_bytes_received"sv),
      },
      profile_{
          .parse = create_metrics(name_, "parse"sv),
          .error = create_metrics(name_, "error"sv),
          .result = create_metrics(name_, "result"sv),
          .agg_trade = create_metrics(name_, "agg_trade"sv),
          .trade = create_metrics(name_, "trade"sv),
          .mini_ticker = create_metrics(name_, "mini_ticker"sv),
          .book_ticker = create_metrics(name_, "book_ticker"sv),
          .depth = create_metrics(name_, "depth"sv),
          .depth_update = create_metrics(name_, "depth_update"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      shared_{shared} {
}

void MarketData::operator()(Event<Start> const &) {
  (*connection_).start();
}

void MarketData::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void MarketData::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  if ((*connection_).ready())
    check_subscribe_queue(now);
}

void MarketData::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      .write(counter_.total_bytes_received, metrics::COUNTER)
      // profile
      .write(profile_.parse, metrics::PROFILE)
      .write(profile_.error, metrics::PROFILE)
      .write(profile_.result, metrics::PROFILE)
      .write(profile_.agg_trade, metrics::PROFILE)
      .write(profile_.trade, metrics::PROFILE)
      .write(profile_.mini_ticker, metrics::PROFILE)
      .write(profile_.book_ticker, metrics::PROFILE)
      .write(profile_.depth, metrics::PROFILE)
      .write(profile_.depth_update, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void MarketData::subscribe(size_t start_from) {
  if (ready())
    subscribe(shared_.symbols.get_slice(index_, start_from));
}

void MarketData::operator()(web::socket::Client::Connected const &) {
}

void MarketData::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  subscribe_queue_.clear();
}

void MarketData::operator()(web::socket::Client::Ready const &) {
  (*this)(ConnectionStatus::READY);
  subscribe();
}

void MarketData::operator()(web::socket::Client::Close const &) {
}

void MarketData::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  const ExternalLatency external_latency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void MarketData::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
  counter_.total_bytes_received.update((*connection_).total_bytes_received());
}

void MarketData::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    const StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::WS,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

void MarketData::subscribe(std::span<Symbol const> const &symbols) {
  if (std::empty(symbols))
    return;
  if (Flags::ws_subscribe_trade_details())
    subscribe(symbols, "trade"sv);
  else
    subscribe(symbols, "aggTrade"sv);
  subscribe(symbols, "miniTicker"sv);
  subscribe(symbols, "bookTicker"sv);
  auto frequency = std::chrono::duration_cast<std::chrono::milliseconds>(Flags::ws_subscribe_depth_freq());
  auto depth = fmt::format(R"(depth@{}ms)"sv, frequency.count());
  subscribe(symbols, depth);
}

void MarketData::subscribe(std::span<Symbol const> const &symbols, std::string_view const &channel) {
  assert(!std::empty(symbols));
  auto id = ++request_id_;
  auto separator = fmt::format(R"(@{}",")"sv, channel);
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}@{}"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, separator),
      channel,
      id);
  log::debug("message={}"sv, message);
  subscribe_queue_.emplace_back(message);
}

void MarketData::parse(std::string_view const &message) {
  profile_.parse([&]() {
    try {
      TraceInfo trace_info;
      core::json::Buffer buffer{decode_buffer_};
      json::MarketStreamParser::dispatch(*this, message, buffer, trace_info);
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void MarketData::operator()(Trace<json::Error> const &event, int32_t id) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::warn("error={}, id={}"sv, error, id);
  });
}

void MarketData::operator()(Trace<json::Result> const &event, int32_t id) {
  profile_.result([&]() {
    auto &[trace_info, result] = event;
    log::info("error={}, id={}"sv, result, id);
  });
}

void MarketData::operator()(Trace<json::AggTrade> const &event) {
  profile_.agg_trade([&]() {
    auto &[trace_info, agg_trade] = event;
    log::info<3>("agg_trade={}"sv, agg_trade);
    (*connection_).touch(trace_info.source_receive_time);
    auto side = agg_trade.buyer_is_maker ? Side::BUY : Side::SELL;
    Trade trade{
        .side = side,
        .price = agg_trade.price,
        .quantity = agg_trade.quantity,
        .trade_id = {},
        .taker_order_id = {},
        .maker_order_id = {},
    };
    core::charconv::to_string(std::back_inserter(trade.trade_id), agg_trade.agg_trade_id);
    const TradeSummary trade_summary{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = agg_trade.symbol,
        .trades = {&trade, 1},
        .exchange_time_utc = agg_trade.event_time,
        .exchange_sequence = {},
    };
    create_trace_and_dispatch(handler_, trace_info, trade_summary, true);
  });
}

void MarketData::operator()(Trace<json::Trade> const &event) {
  profile_.trade([&]() {
    auto &[trace_info, trade] = event;
    log::info<3>("trade={}"sv, trade);
    (*connection_).touch(trace_info.source_receive_time);
    auto side = trade.buyer_is_maker ? Side::BUY : Side::SELL;
    Trade trade_{
        .side = side,
        .price = trade.price,
        .quantity = trade.quantity,
        .trade_id = {},
        .taker_order_id = {},
        .maker_order_id = {},
    };
    core::charconv::to_string(std::back_inserter(trade_.trade_id), trade.trade_id);
    const TradeSummary trade_summary{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = trade.symbol,
        .trades = {&trade_, 1},
        .exchange_time_utc = trade.event_time,
        .exchange_sequence = {},
    };
    create_trace_and_dispatch(handler_, trace_info, trade_summary, true);
  });
}

void MarketData::operator()(Trace<json::MiniTicker> const &event) {
  profile_.mini_ticker([&]() {
    auto &[trace_info, mini_ticker] = event;
    log::info<3>("mini_ticker={}"sv, mini_ticker);
    (*connection_).touch(trace_info.source_receive_time);
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
    const StatisticsUpdate statistics_update{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = mini_ticker.symbol,
        .statistics = statistics,
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = mini_ticker.event_time,
    };
    create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
  });
}

void MarketData::operator()(Trace<json::BookTicker> const &event) {
  profile_.book_ticker([&]() {
    auto &[trace_info, book_ticker] = event;
    log::info<3>("book_ticker={}"sv, book_ticker);
    (*connection_).touch(trace_info.source_receive_time);
    const TopOfBook top_of_book{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = book_ticker.symbol,
        .layer{
            .bid_price = book_ticker.best_bid_price,
            .bid_quantity = book_ticker.best_bid_qty,
            .ask_price = book_ticker.best_ask_price,
            .ask_quantity = book_ticker.best_ask_qty,
        },
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = {},
        .exchange_sequence = book_ticker.order_book_update_id,
    };
    create_trace_and_dispatch(handler_, trace_info, top_of_book, true);
  });
}

void MarketData::operator()(Trace<json::Depth> const &, [[maybe_unused]] std::string_view const &symbol) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::DepthUpdate> const &event) {
  profile_.depth_update([&]() {
    auto &trace_info = event.trace_info;
    auto &depth_update = event.value;
    log::info<3>(R"(depth_update={})"sv, depth_update);
    (*connection_).touch(trace_info.source_receive_time);
    auto symbol = depth_update.symbol;
    auto first_sequence = depth_update.first_update_id;
    auto last_sequence = depth_update.final_update_id;
    auto previous_sequence = first_sequence - 1;
    auto &collector = shared_.mbp_collector[symbol];
    auto create_mbp_update = []<typename T>(T &result, auto const &value) {
      new (&result) T{
          .price = value.price,
          .quantity = value.qty,
          .implied_quantity = NaN,
          .number_of_orders = {},
          .update_action = {},
          .price_level = {},
      };
    };
    core::back_emplacer bids(shared_.bids), asks(shared_.asks);
    for (auto &item : depth_update.bids)
      bids.emplace_back([&](auto &result) { create_mbp_update(result, item); });
    for (auto &item : depth_update.asks)
      asks.emplace_back([&](auto &result) { create_mbp_update(result, item); });
    try {
      auto create_update = [&](auto &bids, auto &asks, auto update_type, auto exchange_sequence) {
        return MarketByPriceUpdate{
            .stream_id = stream_id_,
            .exchange = Flags::exchange(),
            .symbol = symbol,
            .bids = bids,
            .asks = asks,
            .update_type = update_type,
            .exchange_time_utc = depth_update.event_time,
            .exchange_sequence = exchange_sequence,
            .price_decimals = {},
            .quantity_decimals = {},
            .checksum = {},
        };
      };
      auto publish_update = [&](auto &bids, auto &asks) {
        // log::debug(R"(PUBLISH UPDATE symbol="{}")"sv, symbol);
        auto market_by_price_update = create_update(bids, asks, UpdateType::INCREMENTAL, last_sequence);
        create_trace_and_dispatch(handler_, trace_info, market_by_price_update, true, false);
      };
      auto publish_snapshot = [&](auto &bids, auto &asks, auto sequence) {
        log::debug(R"(PUBLISH SNAPSHOT symbol="{}", sequence={})"sv, symbol, sequence);
        auto market_by_price_update = create_update(bids, asks, UpdateType::SNAPSHOT, collector.last_sequence());
        Trace event{trace_info, market_by_price_update};
        shared_(event, true, [&](auto &market_by_price) { collector.apply(market_by_price, sequence, false); });
      };
      auto request_snapshot = [&](auto retries) {
        log::debug(R"(REQUEST symbol="{}" (retries={}))"sv, symbol, retries);
        if (Flags::ws_mbp_request_max_retries() && Flags::ws_mbp_request_max_retries() < retries) {
          log::fatal(R"(Unexpected: symbol="{}", retries={})"sv, symbol, retries);
        }
        shared_.depth_request_queue.emplace_back(symbol);
      };
      collector(
          bids,
          asks,
          first_sequence,
          last_sequence,
          previous_sequence,
          publish_update,
          publish_snapshot,
          request_snapshot);
    } catch (BadState &) {
      log::warn(R"(RESUBSCRIBE symbol="{}")"sv, symbol);
      // XXX HANS publish stale
      collector.clear();
      shared_.depth_request_queue.emplace_back(symbol);
    }
  });
}

void MarketData::check_subscribe_queue(std::chrono::nanoseconds now) {
  subscribe_queue_.dispatch(
      [&](auto now) { return shared_.rate_limiter.can_request(now); },
      [&](auto &message) {
        log::debug(R"(Subscribe: "{}")"sv, message);
        (*connection_).send_text(message);
      },
      now);
}

}  // namespace binance
}  // namespace roq
