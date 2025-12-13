/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/json/market_stream_parser.hpp"

#include <algorithm>
#include <string>

#include "roq/logging.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/field.hpp"
#include "roq/binance/json/stream.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

// === HELPERS ===

namespace {
template <typename T, typename... Args>
void dispatch_helper(auto &handler, auto &value, auto &buffer_stack, auto &trace_info, Args &&...args) {
  T obj{value, buffer_stack};
  create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
}

// <symbol>@<stream>[@<freq>]
auto parse_stream(auto &value) {
  auto full_name = std::get<std::string_view>(value);
  auto idx0 = full_name.find('@');
  if (idx0 == std::string_view::npos) [[unlikely]] {
    log::fatal(R"(Unexpected: name="{}")"sv, full_name);
  }
  std::string symbol{std::begin(full_name), idx0};
  std::ranges::transform(symbol, std::begin(symbol), [](auto item) { return std::toupper(item); });  // note! convert to uppercase
  auto idx1 = full_name.find('@', idx0 + 1);
  std::string_view name{std::begin(full_name) + idx0 + 1, (idx1 == std::string_view::npos) ? std::size(full_name) - idx0 - 1 : idx1 - idx0 - 1};
  Stream stream{name};
  return std::make_pair(symbol, stream);
}
}  // namespace

// === IMPLEMENTATION ===

bool MarketStreamParser::dispatch(
    MarketStreamParser::Handler &handler,
    std::string_view const &message,
    core::json::BufferStack &buffer_stack,
    TraceInfo const &trace_info,
    bool allow_unknown_event_types) {
  // note! find id, symbol and stream before parsing a specific message
  int64_t id = -1;
  std::string symbol;  // note! alloc
  Stream stream = Stream::UNDEFINED_INTERNAL;
  for (int i = 0; i < 2; ++i) {
    core::json::Parser parser{message};
    auto root = parser.root();
    for (auto [key, value] : std::get<core::json::Object>(root)) {
      Field field{key};
      switch (field) {
        using enum Field::type_t;
        case UNDEFINED_INTERNAL:
          log::fatal("Unexpected"sv);
          break;
        case UNKNOWN_INTERNAL:
#ifndef NDEBUG
          log::fatal(R"(Unknown key="{}")"sv, key);
#endif
          break;
        case ERROR:
          if (id >= 0) {
            dispatch_helper<Error>(handler, value, buffer_stack, trace_info, id);
            return true;
          }
          break;
        case RESULT:
          if (id >= 0) {
            dispatch_helper<Result>(handler, value, buffer_stack, trace_info, id);
            return true;
          }
          break;
        case ID:
          id = std::get<decltype(id)>(value);
          break;
        case STREAM:
          std::tie(symbol, stream) = parse_stream(value);  // note! symbol and stream are parsed simultaneously
          break;
        case DATA:
          switch (stream) {
            using enum Stream::type_t;
            case UNDEFINED_INTERNAL:
              break;  // note! need one more iteration
            case UNKNOWN_INTERNAL:
              if (allow_unknown_event_types) {
                return false;
              }
              break;  // ... might take another iteration, probably ok
            case AGG_TRADE:
              dispatch_helper<AggTrade>(handler, value, buffer_stack, trace_info);
              return true;
            case TRADE:
              dispatch_helper<Trade>(handler, value, buffer_stack, trace_info);
              return true;
            case MINI_TICKER:
              dispatch_helper<MiniTicker>(handler, value, buffer_stack, trace_info);
              return true;
            case BOOK_TICKER:
              dispatch_helper<BookTicker>(handler, value, buffer_stack, trace_info);
              return true;
            case DEPTH5:
            case DEPTH10:
            case DEPTH20:
              assert(!std::empty(symbol));  // note! must be defined when stream is defined
              dispatch_helper<Depth>(handler, value, buffer_stack, trace_info, symbol);
              return true;
            case DEPTH:
              assert(!std::empty(symbol));  // note! must be defined when stream is defined
              dispatch_helper<DepthUpdate>(handler, value, buffer_stack, trace_info);
              return true;
          }
          break;
      }
    }
  }
  log::fatal(R"(Unexpected: message="{}")"sv, message);
}

}  // namespace json
}  // namespace binance
}  // namespace roq
