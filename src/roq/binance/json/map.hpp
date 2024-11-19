/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <tuple>

#include "roq/api.hpp"

#include "roq/binance/json/order_status.hpp"
#include "roq/binance/json/order_type.hpp"
#include "roq/binance/json/side.hpp"
#include "roq/binance/json/symbol_status.hpp"
#include "roq/binance/json/time_in_force.hpp"

namespace roq {
namespace binance {
namespace json {

template <typename... Args>
struct Map final {
  explicit Map(Args &&...args) : args_{std::forward<Args>(args)...} {}
  explicit Map(Args const &...args) : args_{args...} {}

  Map(Map const &) = delete;

  template <typename R>
  operator R();

 private:
  std::tuple<Args...> const args_;
};

template <typename R, typename... Args>
inline R map(Args &&...args) {
  return static_cast<R>(Map{std::forward<Args>(args)...});
}

}  // namespace json
}  // namespace binance
}  // namespace roq
