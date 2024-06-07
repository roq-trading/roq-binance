/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/json/market_stream_parser.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "roq/compat.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/field.hpp"
#include "roq/binance/json/stream.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

bool MarketStreamParser::dispatch(
    MarketStreamParser::Handler &handler, std::string_view const &message, std::span<std::byte> const &buffer, TraceInfo const &trace_info) {
  int64_t id = -1;
  std::string symbol;  // allocating because we need uppercase
  auto stream = Stream::UNDEFINED__;
  bool dispatched = false;
  for (int i = 0; i < 2 && !dispatched; ++i) {
    core::json::Parser parser{message};
    auto root = parser.root();
    for (auto [key, value] : std::get<core::json::Object>(root)) {
      auto field = Field{key};
      switch (field) {
        using enum Field::type_t;
        case UNDEFINED__:
          log::fatal("Unexpected"sv);
          break;
        case UNKNOWN__:
#ifndef NDEBUG
          log::fatal(R"(Unknown key="{}")"sv, key);
#endif
          // XXX CALLBACK ?????????????
          break;
        case ERROR:
          if (id >= 0) {
            Error error{value};
            Trace event{trace_info, error};
            dispatched = true;
            handler(event, id);
          }
          break;
        case RESULT:
          if (id >= 0) {
            core::json::Buffer buffer_2{buffer};
            Result result{value, buffer_2};
            Trace event{trace_info, result};
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
          symbol = std::string_view{std::begin(full_name), idx0};
          // note! convert to uppercase
          std::transform(std::begin(symbol), std::end(symbol), std::begin(symbol), [](auto c) { return std::toupper(c); });
          auto idx1 = full_name.find('@', idx0 + 1);
          auto name = std::string_view{std::begin(full_name) + idx0 + 1, (idx1 == full_name.npos) ? std::size(full_name) - idx0 - 1 : idx1 - idx0 - 1};
          stream = Stream{name};
          break;
        }
        case DATA:
          switch (stream) {
            using enum Stream::type_t;
            case UNDEFINED__:
              break;  // wait
            case UNKNOWN__:
#ifndef NDEBUG
              log::fatal("Unexpected (unknown stream)"sv);
#endif
              return false;
            case AGG_TRADE: {
              AggTrade agg_trade{value};
              dispatched = true;
              Trace event{trace_info, agg_trade};
              handler(event);
              break;
            }
            case TRADE: {
              Trade trade{value};
              dispatched = true;
              Trace event{trace_info, trade};
              handler(event);
              break;
            }
            case MINI_TICKER: {
              MiniTicker mini_ticker{value};
              dispatched = true;
              Trace event{trace_info, mini_ticker};
              handler(event);
              break;
            }
            case BOOK_TICKER: {
              BookTicker book_ticker{value};
              dispatched = true;
              Trace event{trace_info, book_ticker};
              handler(event);
              break;
            }
            case DEPTH5:
            case DEPTH10:
            case DEPTH20: {
              assert(!std::empty(symbol));
              core::json::Buffer buffer_2{buffer};
              Depth depth{value, buffer_2};
              dispatched = true;
              Trace event{trace_info, depth};
              handler(event, symbol);
              break;
            }
            case DEPTH: {
              assert(!std::empty(symbol));
              core::json::Buffer buffer_2{buffer};
              DepthUpdate depth_update{value, buffer_2};
              dispatched = true;
              Trace event{trace_info, depth_update};
              handler(event);
              break;
            }
          }
          break;
      }
    }
  }
  return dispatched;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
