/* Copyright (c) 2017-2023, Hans Erik Thrane */

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
};

}  // namespace binance
}  // namespace roq
