/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include "roq/binance/protocol/json/order_status.hpp"
#include "roq/binance/protocol/json/order_type.hpp"
#include "roq/binance/protocol/json/side.hpp"
#include "roq/binance/protocol/json/symbol_status.hpp"
#include "roq/binance/protocol/json/time_in_force.hpp"

#include "roq/order_status.hpp"
#include "roq/order_type.hpp"
#include "roq/side.hpp"
#include "roq/trading_status.hpp"

#include "roq/map.hpp"

namespace roq {

template <>
template <>
std::optional<OrderStatus> Map<binance::protocol::json::OrderStatus>::helper() const;

template <>
template <>
std::optional<OrderType> Map<binance::protocol::json::OrderType>::helper() const;

template <>
template <>
std::optional<Side> Map<binance::protocol::json::Side>::helper() const;

template <>
template <>
std::optional<TradingStatus> Map<binance::protocol::json::SymbolStatus>::helper() const;

template <>
template <>
std::optional<TimeInForce> Map<binance::protocol::json::TimeInForce>::helper() const;

// ===

template <>
template <>
std::optional<binance::protocol::json::OrderStatus> Map<OrderStatus>::helper() const;

template <>
template <>
std::optional<binance::protocol::json::OrderType> Map<OrderType>::helper() const;

template <>
template <>
std::optional<binance::protocol::json::Side> Map<Side>::helper() const;

template <>
template <>
std::optional<binance::protocol::json::TimeInForce> Map<TimeInForce>::helper() const;

}  // namespace roq
