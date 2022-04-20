/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/json/market_stream_parser.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "roq/compat.hpp"

#include "roq/binance/json/field.hpp"
#include "roq/binance/json/stream.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

void MarketStreamParser::dispatch(
    MarketStreamParser::Handler &handler,
    const std::string_view &message,
    core::json::Buffer &buffer,
    const TraceInfo &trace_info) {
  int64_t id = -1;
  std::string symbol;  // allocating because we need uppercase
  auto stream = Stream::UNDEFINED;
  bool dispatched = false;
  for (int i = 0; i < 2 && !dispatched; ++i) {
    core::json::Parser parser(message);
    auto root = parser.root();
    for (auto [key, value] : std::get<core::json::object_t>(root)) {
      auto field = Field(key);
      switch (field) {
        using enum Field::type_t;
        case UNDEFINED:
          log::fatal("Unexpected"sv);
          break;
        case UNKNOWN:
#ifndef NDEBUG
          log::fatal(R"(Unknown key="{}")"sv, key);
#endif
          // XXX CALLBACK ?????????????
          break;
        case ERROR:
          if (id >= 0) {
            Error error(value);
            Trace event(trace_info, error);
            dispatched = true;
            handler(event, id);
          }
          break;
        case RESULT:
          if (id >= 0) {
            Result result(value, buffer);
            Trace event(trace_info, result);
            dispatched = true;
            handler(event, id);
          }
          break;
        case ID:
          id = std::get<decltype(id)>(value);
          break;
        case STREAM: {
          // <symbol>@<stream>[@<freq>]
          auto full_name = std::get<std::string_view>(value);
          auto idx0 = full_name.find('@');  // <symbol>@<stream>
          if (idx0 == full_name.npos) [[unlikely]]
            log::fatal(R"(Unexpected: name="{}")"sv, full_name);
          symbol = std::string_view(std::begin(full_name), idx0);
          // note! convert to uppercase
          std::transform(std::begin(symbol), std::end(symbol), std::begin(symbol), [](auto c) {
            return std::toupper(c);
          });
          auto idx1 = full_name.find('@', idx0 + 1);
          auto name = std::string_view(
              std::begin(full_name) + idx0 + 1,
              (idx1 == full_name.npos) ? std::size(full_name) - idx0 - 1 : idx1 - idx0 - 1);
          stream = Stream(name);
          break;
        }
        case DATA:
          switch (stream) {
            using enum Stream::type_t;
            case UNDEFINED:
              break;  // wait
            case UNKNOWN:
#ifndef NDEBUG
              log::fatal("Unexpected (unknown stream)"sv);
#endif
              return;
            case AGG_TRADE: {
              AggTrade agg_trade(value);
              dispatched = true;
              Trace event(trace_info, agg_trade);
              handler(event);
              break;
            }
            case TRADE: {
              Trade trade(value);
              dispatched = true;
              Trace event(trace_info, trade);
              handler(event);
              break;
            }
            case MINI_TICKER: {
              MiniTicker mini_ticker(value);
              dispatched = true;
              Trace event(trace_info, mini_ticker);
              handler(event);
              break;
            }
            case BOOK_TICKER: {
              BookTicker book_ticker(value);
              dispatched = true;
              Trace event(trace_info, book_ticker);
              handler(event);
              break;
            }
            case DEPTH5:
            case DEPTH10:
            case DEPTH20: {
              assert(!std::empty(symbol));
              Depth depth(value, buffer);
              dispatched = true;
              Trace event(trace_info, depth);
              handler(event, symbol);
              break;
            }
            case DEPTH: {
              assert(!std::empty(symbol));
              DepthUpdate depth_update(value, buffer);
              dispatched = true;
              Trace event(trace_info, depth_update);
              handler(event);
              break;
            }
          }
          break;
      }
    }
  }
  if (dispatched)
    return;
  log::warn(R"(message="{}")"sv, message);
  log::fatal("Unexpected"sv);
}

}  // namespace json
}  // namespace binance
}  // namespace roq
