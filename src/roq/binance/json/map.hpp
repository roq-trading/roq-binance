/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include "roq/binance/json/order_status.hpp"
#include "roq/binance/json/order_type.hpp"
#include "roq/binance/json/side.hpp"
#include "roq/binance/json/symbol_status.hpp"
#include "roq/binance/json/time_in_force.hpp"

#include "roq/order_status.hpp"
#include "roq/order_type.hpp"
#include "roq/side.hpp"
#include "roq/trading_status.hpp"

#include "roq/map.hpp"

namespace roq {

template <>
template <>
std::optional<OrderStatus> Map<binance::json::OrderStatus>::helper() const;

template <>
template <>
std::optional<OrderType> Map<binance::json::OrderType>::helper() const;

template <>
template <>
std::optional<Side> Map<binance::json::Side>::helper() const;

template <>
template <>
std::optional<TradingStatus> Map<binance::json::SymbolStatus>::helper() const;

template <>
template <>
std::optional<TimeInForce> Map<binance::json::TimeInForce>::helper() const;

// ===

template <>
template <>
std::optional<binance::json::OrderStatus> Map<OrderStatus>::helper() const;

template <>
template <>
std::optional<binance::json::OrderType> Map<OrderType>::helper() const;

template <>
template <>
std::optional<binance::json::Side> Map<Side>::helper() const;

template <>
template <>
std::optional<binance::json::TimeInForce> Map<TimeInForce>::helper() const;

}  // namespace roq
