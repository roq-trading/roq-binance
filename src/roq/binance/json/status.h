/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include "roq/core/json/buffer.h"

#include "roq/binance/json/currency.h"
#include "roq/binance/json/product.h"


namespace roq {
namespace binance {
namespace json {

struct Status final {
  struct {
    Currency *items = nullptr;
    size_t length = 0;
  } currencies;
  struct {
    Product *items = nullptr;
    size_t length = 0;
  } products;

  static Status parse(
      const std::string_view& message,
      core::json::Buffer& buffer);
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::Status> {
  template <typename C>
  constexpr auto parse(C& ctx) {
    return ctx.begin();
  }
  template <typename C>
  auto format(const roq::binance::json::Status& value, C& ctx) {
    return format_to(
        ctx.out(),
        "{{"
        "products=[{}], "
        "currencies=[{}]"
        "}}",
        fmt::join(
          value.products.items,
          value.products.items + value.products.length,
          ", "),
        fmt::join(
          value.currencies.items,
          value.currencies.items + value.currencies.length,
          ", "));
  }
};
