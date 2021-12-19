/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/market_data.h"

#include <algorithm>

#include "roq/utils/mask.h"
#include "roq/utils/safe_cast.h"
#include "roq/utils/update.h"

#include "roq/core/back_emplacer.h"
#include "roq/core/charconv.h"

#include "roq/core/tools/exception.h"

#include "roq/core/metrics/factory.h"

#include "roq/binance/flags.h"

using namespace std::literals;

namespace roq {
namespace binance {

namespace {
static const auto NAME = "md"sv;
static const auto SUPPORTS = utils::Mask{
    SupportType::TOP_OF_BOOK,
    SupportType::MARKET_BY_PRICE,
    SupportType::TRADE_SUMMARY,
    SupportType::STATISTICS,
};

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

MarketData::MarketData(
    Handler &handler, core::io::Context &context, uint32_t stream_id, Shared &shared, size_t index)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      index_(index), connection_(
                         *this,
                         context,
                         core::URI(Flags::ws_uri()),
                         std::string_view{},  // query
                         Flags::ws_ping_freq(),
                         Flags::decode_buffer_size(),
                         Flags::encode_buffer_size(),
                         []() { return std::string(); }),
      decode_buffer_(Flags::decode_buffer_size()),
      request_id_(static_cast<uint64_t>(stream_id_) * 1000000),  // scale (debugging)
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
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
      shared_(shared) {
}

void MarketData::operator()(const Event<Start> &) {
  connection_.start();
}

void MarketData::operator()(const Event<Stop> &) {
  connection_.stop();
}

void MarketData::operator()(const Event<Timer> &event) {
  auto now = event.value.now;
  connection_.refresh(now);
  if (connection_.ready())
    check_subscribe_queue(now);
}

void MarketData::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
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

void MarketData::operator()(const core::web::ClientSocket::Connected &) {
}

void MarketData::operator()(const core::web::ClientSocket::Disconnected &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
}

void MarketData::operator()(const core::web::ClientSocket::Ready &) {
  (*this)(ConnectionStatus::READY);
  subscribe();
}

void MarketData::operator()(const core::web::ClientSocket::Close &) {
}

void MarketData::operator()(const core::web::ClientSocket::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void MarketData::operator()(const core::web::ClientSocket::Text &text) {
  parse(text.payload);
}

void MarketData::operator()(const core::web::ClientSocket::Binary &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::WEB_SOCKET,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"sv, stream_status);
    server::create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

void MarketData::subscribe(const roq::span<std::string const> &symbols) {
  if (Flags::ws_subscribe_trade_details()) {
    subscribe_trade(symbols);
  } else {
    subscribe_agg_trade(symbols);
  }
  subscribe_mini_ticker(symbols);
  subscribe_book_ticker(symbols);
  subscribe_diff_depth(symbols);
}

void MarketData::subscribe_agg_trade(const roq::span<std::string const> &symbols) {
  assert(!std::empty(symbols));
  auto id = ++request_id_;
  auto now = core::get_system_clock();
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}@aggTrade"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, R"(@aggTrade",")"sv),
      id);
  log::debug("message={}"sv, message);
  subscribe_queue_.emplace_back(now, message);
}

void MarketData::subscribe_trade(const roq::span<std::string const> &symbols) {
  assert(!std::empty(symbols));
  auto id = ++request_id_;
  auto now = core::get_system_clock();
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}@trade"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, R"(@trade",")"sv),
      id);
  log::debug("message={}"sv, message);
  subscribe_queue_.emplace_back(now, message);
}

void MarketData::subscribe_mini_ticker(const roq::span<std::string const> &symbols) {
  assert(!std::empty(symbols));
  auto id = ++request_id_;
  auto now = core::get_system_clock();
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}@miniTicker"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, R"(@miniTicker",")"sv),
      id);
  log::debug("message={}"sv, message);
  subscribe_queue_.emplace_back(now, message);
}

void MarketData::subscribe_book_ticker(const roq::span<std::string const> &symbols) {
  assert(!std::empty(symbols));
  auto id = ++request_id_;
  auto now = core::get_system_clock();
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}@bookTicker"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, R"(@bookTicker",")"sv),
      id);
  log::debug("message={}"sv, message);
  subscribe_queue_.emplace_back(now, message);
}

void MarketData::subscribe_diff_depth(const roq::span<std::string const> &symbols) {
  assert(!std::empty(symbols));
  std::chrono::milliseconds frequency = utils::safe_cast(Flags::ws_subscribe_depth_freq());
  auto stream = fmt::format(R"(@depth@{}ms)"sv, frequency.count());
  auto id = ++request_id_;
  auto now = core::get_system_clock();
  auto separator = fmt::format(R"({}",")"sv, stream);
  auto message = fmt::format(
      R"({{)"
      R"("method":"SUBSCRIBE",)"
      R"("params":["{}{}"],)"
      R"("id":{})"
      R"(}})"sv,
      fmt::join(symbols, separator),
      stream,
      id);
  log::debug("message={}"sv, message);
  subscribe_queue_.emplace_back(now, message);
}

void MarketData::parse(const std::string_view &message) {
  profile_.parse([&]() {
    try {
      auto trace_info = server::create_trace_info();
      core::json::Buffer buffer(decode_buffer_);
      json::MarketStreamParser::dispatch(*this, message, buffer, trace_info);
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void MarketData::operator()(const server::Trace<json::Error> &event, int32_t id) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::warn("error={}, id={}"sv, error, id);
  });
}

void MarketData::operator()(const server::Trace<json::Result> &event, int32_t id) {
  profile_.result([&]() {
    auto &[trace_info, result] = event;
    log::info("error={}, id={}"sv, result, id);
  });
}

void MarketData::operator()(const server::Trace<json::AggTrade> &event) {
  profile_.agg_trade([&]() {
    auto &[trace_info, agg_trade] = event;
    log::info<3>("agg_trade={}"sv, agg_trade);
    auto side = agg_trade.buyer_is_maker ? Side::BUY : Side::SELL;
    Trade trade{
        .side = side,
        .price = agg_trade.price,
        .quantity = agg_trade.quantity,
        .trade_id = {},
    };
    core::charconv::to_string(std::back_inserter(trade.trade_id), agg_trade.agg_trade_id);
    const TradeSummary trade_summary{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = agg_trade.symbol,
        .trades = {&trade, 1},
        .exchange_time_utc = agg_trade.event_time,
    };
    create_trace_and_dispatch(handler_, trace_info, trade_summary, true);
  });
}

void MarketData::operator()(const server::Trace<json::Trade> &event) {
  profile_.trade([&]() {
    auto &[trace_info, trade] = event;
    log::info<3>("trade={}"sv, trade);
    auto side = trade.buyer_is_maker ? Side::BUY : Side::SELL;
    Trade trade_{
        .side = side,
        .price = trade.price,
        .quantity = trade.quantity,
        .trade_id = {},
    };
    core::charconv::to_string(std::back_inserter(trade_.trade_id), trade.trade_id);
    const TradeSummary trade_summary{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = trade.symbol,
        .trades = {&trade_, 1},
        .exchange_time_utc = trade.event_time,
    };
    create_trace_and_dispatch(handler_, trace_info, trade_summary, true);
  });
}

void MarketData::operator()(const server::Trace<json::MiniTicker> &event) {
  profile_.mini_ticker([&]() {
    auto &[trace_info, mini_ticker] = event;
    log::info<3>("mini_ticker={}"sv, mini_ticker);
    Statistics statistics[] = {
        {.type = StatisticsType::HIGHEST_TRADED_PRICE, .value = mini_ticker.high_price},
        {.type = StatisticsType::LOWEST_TRADED_PRICE, .value = mini_ticker.low_price},
        {.type = StatisticsType::OPEN_PRICE, .value = mini_ticker.open_price},
        {.type = StatisticsType::CLOSE_PRICE, .value = mini_ticker.close_price},
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

void MarketData::operator()(const server::Trace<json::BookTicker> &event) {
  profile_.book_ticker([&]() {
    auto &[trace_info, book_ticker] = event;
    log::info<3>("book_ticker={}"sv, book_ticker);
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
    };
    create_trace_and_dispatch(handler_, trace_info, top_of_book, true);
  });
}

void MarketData::operator()(
    const server::Trace<json::Depth> &, [[maybe_unused]] const std::string_view &symbol) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(const server::Trace<json::DepthUpdate> &event) {
  profile_.depth_update([&]() {
    // auto &[trace_info, depth_update] = event;
    auto &trace_info = event.trace_info;
    auto &depth_update = event.value;
    log::info<3>(R"(depth_update={})"sv, depth_update);
    auto symbol = depth_update.symbol;
    auto first_sequence = depth_update.first_update_id;
    auto last_sequence = depth_update.final_update_id;
    auto previous_sequence = first_sequence - 1;
    auto &collector = shared_.mbp_collector[symbol];
    core::back_emplacer bids(shared_.bids), asks(shared_.asks);
    for (auto &item : depth_update.bids)
      bids.emplace_back([&item](auto &result) { emplace(result, item); });
    for (auto &item : depth_update.asks)
      asks.emplace_back([&item](auto &result) { emplace(result, item); });
    auto exchange_time_utc = depth_update.event_time;
    try {
      collector(
          bids,
          asks,
          first_sequence,
          last_sequence,
          previous_sequence,
          [&](auto &bids, auto &asks) {  // update
            // log::debug(R"(PUBLISH UPDATE symbol="{}")"sv, symbol);
            MarketByPriceUpdate market_by_price_update{
                .stream_id = stream_id_,
                .exchange = Flags::exchange(),
                .symbol = symbol,
                .bids = bids,
                .asks = asks,
                .update_type = UpdateType::INCREMENTAL,
                .exchange_time_utc = exchange_time_utc,
                .exchange_sequence = last_sequence,
                .price_decimals = {},
                .quantity_decimals = {},
                .checksum = {},
            };
            server::create_trace_and_dispatch(
                handler_, trace_info, market_by_price_update, true, false);
          },
          [&](auto &bids, auto &asks, auto sequence) {  // snapshot
            log::debug(R"(PUBLISH SNAPSHOT symbol="{}", sequence={})"sv, symbol, sequence);
            MarketByPriceUpdate market_by_price_update{
                .stream_id = stream_id_,
                .exchange = Flags::exchange(),
                .symbol = symbol,
                .bids = bids,
                .asks = asks,
                .update_type = UpdateType::SNAPSHOT,
                .exchange_time_utc = exchange_time_utc,
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
            if (retries > Flags::ws_mbp_request_max_retries()) {
              log::fatal("Unexpected"sv);
            }
            auto now = trace_info.source_receive_time;
            shared_.request_queue.emplace_back(now + Flags::ws_mbp_request_delay(), symbol);
          });
    } catch (BadState &) {
      log::warn(R"(RESUBSCRIBE symbol="{}")"sv, symbol);
      // XXX HANS publish stale
      collector.clear();
      auto now = trace_info.source_receive_time;
      shared_.request_queue.emplace_back(now + Flags::ws_mbp_request_delay(), symbol);
    }
  });
}

void MarketData::check_subscribe_queue(std::chrono::nanoseconds now) {
  while (!std::empty(subscribe_queue_)) {
    auto &tmp = subscribe_queue_.front();
    if (now < tmp.first)
      break;
    if (shared_.can_request(now, [&]() {
          auto &message = tmp.second;
          log::debug(R"(Subscribe: "{}")"sv, message);
          connection_.send_text(message);
          subscribe_queue_.pop_front();
        })) {
    } else {
      return;
    }
  }
}

}  // namespace binance
}  // namespace roq
