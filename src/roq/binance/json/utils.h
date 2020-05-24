/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <chrono>

#include "roq/core/utility.h"

#include "roq/core/json/parser.h"

#include "roq/core/charconv/datetime.h"

#include "roq/binance/json/event_type.h"
#include "roq/binance/json/symbol_status.h"

namespace roq {
namespace binance {
namespace json {

template <typename T>
inline void update(
    T& result,
    const core::json::value_t& value) {
  result = core::json::get<T>(value);
}

template <>
inline void update(
    EventType& result,
    const core::json::value_t& value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

template <>
inline void update(
    SymbolStatus& result,
    const core::json::value_t& value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

template <>
inline void update(
    std::chrono::nanoseconds& result,
    const core::json::value_t& value) {
  return std::visit(
      overloaded {
        [&](const core::json::null_t&) {
          result = std::chrono::nanoseconds {};
        },
        [](bool) {
          throw std::bad_cast();
        },
        [&](int64_t value) {
          result = std::chrono::milliseconds { value };
        },
        [&](double value) {
          result = std::chrono::nanoseconds {
            static_cast<uint64_t>(value * 1.0e9)
          };
        },
        [&](const std::string_view& value) {
          result = core::charconv::to_datetime(value);
        },
        [](const core::json::object_t&) {
          throw std::bad_cast();
        },
        [](const core::json::array_t&) {
          throw std::bad_cast();
        },
      },
      value);
}

inline roq::TradingStatus map(json::SymbolStatus symbol_status) {
  switch (symbol_status) {
    case json::SymbolStatus::UNDEFINED:     break;
    case json::SymbolStatus::UNKNOWN:       break;
    case json::SymbolStatus::PRE_TRADING:   return roq::TradingStatus::CLOSED;
    case json::SymbolStatus::TRADING:       return roq::TradingStatus::OPEN;
    case json::SymbolStatus::POST_TRADING:  return roq::TradingStatus::CLOSED;
    case json::SymbolStatus::END_OF_DAY:    return roq::TradingStatus::CLOSED;
    case json::SymbolStatus::HALT:          return roq::TradingStatus::CLOSED;
    case json::SymbolStatus::AUCTION_MATCH: return roq::TradingStatus::CLOSED;
    case json::SymbolStatus::BREAK:         return roq::TradingStatus::CLOSED;
  }
  return roq::TradingStatus::UNDEFINED;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
