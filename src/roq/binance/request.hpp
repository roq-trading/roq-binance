/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <chrono>

namespace roq {
namespace binance {

struct Request final {
  // spot:
  // account
  std::chrono::nanoseconds request_account = {};
  std::chrono::nanoseconds respond_account = {};
  // orders
  std::chrono::nanoseconds request_orders = {};
  std::chrono::nanoseconds respond_orders = {};
  // trades
  std::chrono::nanoseconds request_trades = {};
  std::chrono::nanoseconds respond_trades = {};
  // margin cross:
  // account
  std::chrono::nanoseconds request_account_cross = {};
  std::chrono::nanoseconds respond_account_cross = {};
  // orders
  std::chrono::nanoseconds request_orders_cross = {};
  std::chrono::nanoseconds respond_orders_cross = {};
  // trades
  std::chrono::nanoseconds request_trades_cross = {};
  std::chrono::nanoseconds respond_trades_cross = {};
};

}  // namespace binance
}  // namespace roq
