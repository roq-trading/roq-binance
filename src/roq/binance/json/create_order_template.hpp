/* Copyright (c) 2017-2026, Hans Erik Thrane */

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
  constexpr auto parse(format_parse_context &context) { return std::begin(context); }
  auto format(roq::binance::json::CreateOrderTemplate const &value, format_context &context) const {
    using namespace std::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(self_trade_prevention_mode={})"
        R"(}})"sv,
        value.self_trade_prevention_mode);
  }
};
