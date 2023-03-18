/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <vector>

#include "roq/oms/order.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/core/charconv/datetime.hpp"

#include "roq/web/http/status.hpp"

#include "roq/binance/json/order_status.hpp"
#include "roq/binance/json/order_type.hpp"
#include "roq/binance/json/side.hpp"
#include "roq/binance/json/symbol_status.hpp"
#include "roq/binance/json/time_in_force.hpp"

namespace roq {
namespace binance {
namespace json {

template <typename T>
inline void update(T &result, core::json::Value const &value) {
  result = core::json::get<T>(value);
}

template <>
inline void update(std::chrono::milliseconds &result, core::json::Value const &value) {
  return std::visit(
      overloaded{
          [&](core::json::Null const &) { result = std::chrono::milliseconds{}; },
          [](bool) { throw std::bad_cast{}; },
          [&](int64_t val) { result = std::chrono::milliseconds{val}; },
          [&](double val) { result = std::chrono::milliseconds{static_cast<int64_t>(val)}; },
          [&](std::string_view const &val) {
            result = core::charconv::datetime_from_string<std::remove_reference<decltype(result)>::type>(val);
          },
          [](core::json::Object const &) { throw std::bad_cast{}; },
          [](core::json::Array const &) { throw std::bad_cast{}; },
      },
      value);
}

inline roq::OrderStatus map(json::OrderStatus side) {
  switch (side) {
    using enum json::OrderStatus::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case NEW:
      return roq::OrderStatus::WORKING;
    case PARTIALLY_FILLED:
      return roq::OrderStatus::WORKING;
    case FILLED:
      return roq::OrderStatus::COMPLETED;
    case CANCELED:
      return roq::OrderStatus::CANCELED;
    case PENDING_CANCEL:  // XXX HANS what do do?
      break;
    case REJECTED:
      return roq::OrderStatus::REJECTED;
    case EXPIRED:
      return roq::OrderStatus::EXPIRED;
  }
  return roq::OrderStatus::UNDEFINED;
}

inline json::OrderStatus map(roq::OrderStatus value) {
  switch (value) {
    using enum roq::OrderStatus;
    case UNDEFINED:
      break;
    case SENT:
      break;
    case ACCEPTED:
      break;
    case SUSPENDED:
      break;
    case WORKING:
      return json::OrderStatus::NEW;
    case STOPPED:
      break;
    case COMPLETED:
      break;  // XXX HANS no enum for COMPLETED ???
    case EXPIRED:
      break;
    case CANCELED:
      return json::OrderStatus::CANCELED;
    case REJECTED:
      return json::OrderStatus::REJECTED;
  }
  return json::OrderStatus::UNDEFINED;
}

inline roq::OrderType map(json::OrderType side) {
  switch (side) {
    using enum json::OrderType::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case LIMIT:
      return roq::OrderType::LIMIT;
    case MARKET:
      return roq::OrderType::MARKET;
    case STOP_LOSS:
      break;
    case STOP_LOSS_LIMIT:
      break;
    case TAKE_PROFIT:
      break;
    case TAKE_PROFIT_LIMIT:
      break;
    case LIMIT_MAKER:
      return roq::OrderType::LIMIT;
  }
  return roq::OrderType::UNDEFINED;
}

inline json::OrderType map(roq::OrderType side) {
  switch (side) {
    using enum roq::OrderType;
    case UNDEFINED:
      break;
    case MARKET:
      return json::OrderType::MARKET;
    case LIMIT:
      return json::OrderType::LIMIT;
  }
  return json::OrderType::UNDEFINED;
}

inline roq::Side map(json::Side side) {
  switch (side) {
    using enum json::Side::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case BUY:
      return roq::Side::BUY;
    case SELL:
      return roq::Side::SELL;
  }
  return roq::Side::UNDEFINED;
}

inline json::Side map(roq::Side side) {
  switch (side) {
    using enum roq::Side;
    case UNDEFINED:
      break;
    case BUY:
      return json::Side::BUY;
    case SELL:
      return json::Side::SELL;
  }
  return json::Side::UNDEFINED;
}

inline roq::TradingStatus map(json::SymbolStatus symbol_status) {
  switch (symbol_status) {
    using enum json::SymbolStatus::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case TRADING:
      return roq::TradingStatus::OPEN;
    case HALT:
      return roq::TradingStatus::HALT;
    case BREAK:
      return roq::TradingStatus::CLOSE;
    case END_OF_DAY:
      return roq::TradingStatus::END_OF_DAY;
      // note! following probably not used
      // - https://dev.binance.vision/t/explanation-on-symbol-status/118
    case PRE_TRADING:
      return roq::TradingStatus::PRE_OPEN;
    case AUCTION_MATCH:
      return roq::TradingStatus::PRE_OPEN;
    case POST_TRADING:
      return roq::TradingStatus::CLOSE;
  }
  return roq::TradingStatus::UNDEFINED;
}

inline roq::TimeInForce map(json::TimeInForce time_in_force) {
  switch (time_in_force) {
    using enum json::TimeInForce::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case GTC:
      return roq::TimeInForce::GTC;
    case IOC:
      return roq::TimeInForce::IOC;
    case FOK:
      return roq::TimeInForce::FOK;
  }
  return roq::TimeInForce::UNDEFINED;
}

inline json::TimeInForce map(roq::TimeInForce time_in_force) {
  switch (time_in_force) {
    using enum roq::TimeInForce;
    case UNDEFINED:
      break;
    case GFD:
      break;
    case GTC:
      return json::TimeInForce::GTC;
    case OPG:
      break;
    case IOC:
      return json::TimeInForce::IOC;
    case FOK:
      return json::TimeInForce::FOK;
    case GTX:
      break;
    case GTD:
      break;
    case AT_THE_CLOSE:
      break;
    case GOOD_THROUGH_CROSSING:
      break;
    case AT_CROSSING:
      break;
    case GOOD_FOR_TIME:
      break;
    case GFA:
      break;
    case GFM:
      break;
  }
  return json::TimeInForce::UNDEFINED;
}

extern roq::Error guess_error(int32_t code);

extern std::string_view new_order(
    std::vector<char> &buffer,
    CreateOrder const &,
    oms::Order const &,
    std::string_view const &request_id,
    std::chrono::milliseconds recv_window);

extern std::string_view new_order_ws_url(
    std::vector<char> &buffer,
    CreateOrder const &,
    oms::Order const &,
    std::string_view const &request_id,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now);

extern std::string_view new_order_ws_json(
    std::vector<char> &buffer,
    CreateOrder const &,
    oms::Order const &,
    std::string_view const &request_id,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now,
    std::string_view const &signature);

extern std::string_view cancel_replace_order(
    std::vector<char> &buffer,
    std::string_view const &cancel_request_id,
    std::string_view const &cancel_previous_request_id,
    CreateOrder const &,
    oms::Order const &,
    std::string_view const &create_request_id,
    std::chrono::milliseconds recv_window,
    bool stop_on_failure);

extern std::string_view cancel_order(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    oms::Order const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    std::chrono::milliseconds recv_window);

extern std::string_view cancel_order_ws_url(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    oms::Order const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now);

extern std::string_view cancel_order_ws_json(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    oms::Order const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now,
    std::string_view const &signature);

extern std::string_view cancel_all_open_orders(
    std::vector<char> &buffer, std::string_view const &symbol, std::chrono::milliseconds recv_window);

}  // namespace json
}  // namespace binance
}  // namespace roq
