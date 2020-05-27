/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <chrono>

#include "roq/core/utility.h"

#include "roq/core/json/parser.h"

#include "roq/core/charconv/datetime.h"

#include "roq/binance/json/event_type.h"
#include "roq/binance/json/execution_type.h"
#include "roq/binance/json/order_status.h"
#include "roq/binance/json/order_type.h"
#include "roq/binance/json/side.h"
#include "roq/binance/json/symbol_status.h"
#include "roq/binance/json/time_in_force.h"

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
    ExecutionType& result,
    const core::json::value_t& value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

template <>
inline void update(
    OrderStatus& result,
    const core::json::value_t& value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

template <>
inline void update(
    OrderType& result,
    const core::json::value_t& value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

template <>
inline void update(
    Side& result,
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
    TimeInForce& result,
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
          result = std::chrono::milliseconds {
            static_cast<int64_t>(value)
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

inline roq::OrderType map(json::OrderType side) {
  switch (side) {
    case json::OrderType::UNDEFINED:         break;
    case json::OrderType::UNKNOWN:           break;
    case json::OrderType::LIMIT:             return roq::OrderType::LIMIT;
    case json::OrderType::MARKET:            return roq::OrderType::MARKET;
    case json::OrderType::STOP_LOSS:         break;
    case json::OrderType::STOP_LOSS_LIMIT:   break;
    case json::OrderType::TAKE_PROFIT:       break;
    case json::OrderType::TAKE_PROFIT_LIMIT: break;
    case json::OrderType::LIMIT_MAKER:       return roq::OrderType::LIMIT;
  }
  return roq::OrderType::UNDEFINED;
}

inline json::OrderType map(roq::OrderType side) {
  switch (side) {
    case roq::OrderType::UNDEFINED: break;
    case roq::OrderType::MARKET:    return json::OrderType::MARKET;
    case roq::OrderType::LIMIT:     return json::OrderType::LIMIT;
  }
  return json::OrderType::UNDEFINED;
}

inline roq::Side map(json::Side side) {
  switch (side) {
    case json::Side::UNDEFINED: break;
    case json::Side::UNKNOWN:   break;
    case json::Side::BUY:       return roq::Side::BUY;
    case json::Side::SELL:      return roq::Side::SELL;
  }
  return roq::Side::UNDEFINED;
}

inline json::Side map(roq::Side side) {
  switch (side) {
    case roq::Side::UNDEFINED: break;
    case roq::Side::BUY:       return json::Side::BUY;
    case roq::Side::SELL:      return json::Side::SELL;
  }
  return json::Side::UNDEFINED;
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

inline roq::TimeInForce map(json::TimeInForce time_in_force) {
  switch (time_in_force) {
    case json::TimeInForce::UNDEFINED: break;
    case json::TimeInForce::UNKNOWN:   break;
    case json::TimeInForce::GTC:       return roq::TimeInForce::GTC;
    case json::TimeInForce::IOC:       return roq::TimeInForce::IOC;
    case json::TimeInForce::FOK:       return roq::TimeInForce::FOK;
  }
  return roq::TimeInForce::UNDEFINED;
}

inline json::TimeInForce map(roq::TimeInForce time_in_force) {
  switch (time_in_force) {
    case roq::TimeInForce::UNDEFINED: break;
    case roq::TimeInForce::FOK:       return json::TimeInForce::FOK;
    case roq::TimeInForce::IOC:       return json::TimeInForce::IOC;
    case roq::TimeInForce::GFD:       break;
    case roq::TimeInForce::GTC:       return json::TimeInForce::GTC;
  }
  return json::TimeInForce::UNDEFINED;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
