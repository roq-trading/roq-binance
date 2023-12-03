/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include "roq/binance/json/self_trade_prevention_mode.hpp"

namespace roq {
namespace binance {
namespace json {

struct CreateOrderTemplate final {
  SelfTradePreventionMode self_trade_prevention_mode = {};
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::CreateOrderTemplate> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::binance::json::CreateOrderTemplate const &value, Context &context) const {
    using namespace std::literals;
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(self_trade_prevention_mode={})"
        R"(}})"_cf,
        value.self_trade_prevention_mode);
  }
};
