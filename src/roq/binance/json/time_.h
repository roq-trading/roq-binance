/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <chrono>
#include <string_view>

namespace roq {
namespace binance {
namespace json {

struct Time final {
  std::chrono::nanoseconds epoch = {};
  std::chrono::nanoseconds iso = {};

  static Time parse(const std::string_view& message);
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::Time> {
  template <typename C>
  constexpr auto parse(C& ctx) {
    return ctx.begin();
  }
  template <typename C>
  auto format(const roq::binance::json::Time& value, C& ctx) {
    return format_to(
        ctx.out(),
        "{{"
        "epoch={}, "
        "iso={}"
        "}}",
        value.epoch,
        value.iso);
  }
};
