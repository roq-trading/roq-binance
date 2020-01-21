/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include <string_view>

namespace roq {
namespace binance {
namespace json {

struct Error final {
  std::string_view message;

  static Error parse(const std::string_view& message);
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::Error> {
  template <typename C>
  constexpr auto parse(C& ctx) {
    return ctx.begin();
  }
  template <typename C>
  auto format(const roq::binance::json::Error& value, C& ctx) {
    return format_to(
        ctx.out(),
        "{{"
        "message=\"{}\""
        "}}",
        value.message);
  }
};

