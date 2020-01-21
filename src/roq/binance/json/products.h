/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include "roq/core/json/buffer.h"

#include "roq/binance/json/product.h"

namespace roq {
namespace binance {
namespace json {

struct Products final {
  Product *items = nullptr;
  size_t length = 0;

  static Products parse(
      const std::string_view& message,
      core::json::Buffer& buffer);
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::Products> {
  template <typename C>
  constexpr auto parse(C& ctx) {
    return ctx.begin();
  }
  template <typename C>
  auto format(const roq::binance::json::Products& value, C& ctx) {
    return format_to(
        ctx.out(),
        "[{}]",
        fmt::join(value.items, value.items + value.length, ", "));
  }
};
