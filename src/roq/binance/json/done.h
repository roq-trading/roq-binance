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

struct Done final {
  std::string_view order_id;
  double price = std::numeric_limits<double>::quiet_NaN();
  std::string_view product_id;
  api::Reason reason = api::Reason::UNKNOWN;
  double remaining_size = std::numeric_limits<double>::quiet_NaN();
  uint64_t sequence = 0;
  api::Side side = api::Side::UNKNOWN;
  std::chrono::nanoseconds time = {};

  static Done parse(const std::string_view& message);
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::Done> {
  template <typename C>
  constexpr auto parse(C& ctx) {
    return ctx.begin();
  }
  template <typename C>
  auto format(const roq::binance::json::Done& value, C& ctx) {
    return format_to(
        ctx.out(),
        "{{"
        "order_id=\"{}\", "
        "price={}, "
        "product_id=\"{}\", "
        "reason={}, "
        "remaining_size={}, "
        "sequence={}, "
        "side={}, "
        "time={}"
        "}}",
        value.order_id,
        value.price,
        value.product_id,
        value.reason,
        value.remaining_size,
        value.sequence,
        value.side,
        value.time);
  }
};
