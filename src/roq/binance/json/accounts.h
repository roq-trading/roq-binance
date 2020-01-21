/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include "roq/core/json/buffer.h"

#include "roq/binance/json/account.h"

namespace roq {
namespace binance {
namespace json {

struct Accounts final {
  Account *items = nullptr;
  size_t length = 0;

  static Accounts parse(
      const std::string_view& message,
      core::json::Buffer& buffer);

  using account_t = std::remove_pointer<decltype(items)>::type;
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::Accounts> {
  template <typename C>
  constexpr auto parse(C& ctx) {
    return ctx.begin();
  }
  template <typename C>
  auto format(const roq::binance::json::Accounts& value, C& ctx) {
    return format_to(
        ctx.out(),
        "[{}]",
        fmt::join(value.items, value.items + value.length, ", "));
  }
};
