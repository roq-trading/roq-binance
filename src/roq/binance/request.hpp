/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <chrono>

namespace roq {
namespace binance {

struct Request final {
  // account
  std::chrono::nanoseconds request_account = {};
  std::chrono::nanoseconds respond_account = {};
  // orders
  std::chrono::nanoseconds request_orders = {};
  std::chrono::nanoseconds respond_orders = {};
  // trades
  std::chrono::nanoseconds request_trades = {};
  std::chrono::nanoseconds respond_trades = {};
};

}  // namespace binance
}  // namespace roq
