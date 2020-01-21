/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <chrono>
#include <limits>
#include <string_view>

#include "roq/binance/api/enums.h"

namespace roq {
namespace binance {
namespace json {

struct Change final {
  double new_size = std::numeric_limits<double>::quiet_NaN();
  double old_size = std::numeric_limits<double>::quiet_NaN();
  std::string_view order_id;
  double price = std::numeric_limits<double>::quiet_NaN();
  std::string_view product_id;
  uint64_t sequence = 0;
  api::Side side = api::Side::UNKNOWN;
  std::chrono::nanoseconds time = {};

  static Change parse(const std::string_view& message);
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::Change> {
  template <typename C>
  constexpr auto parse(C& ctx) {
    return ctx.begin();
  }
  template <typename C>
  auto format(const roq::binance::json::Change& value, C& ctx) {
    return format_to(
        ctx.out(),
        "{{"
        "new_size={}, "
        "old_size={}, "
        "order_id=\"{}\", "
        "price={}, "
        "product_id=\"{}\", "
        "sequence={}, "
        "side={}, "
        "time={}"
        "}}",
        value.new_size,
        value.old_size,
        value.order_id,
        value.price,
        value.product_id,
        value.sequence,
        value.side,
        value.time);
  }
};
