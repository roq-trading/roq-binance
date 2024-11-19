/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include "roq/binance/json/cancel_replace_mode.hpp"
#include "roq/binance/json/cancel_restrictions.hpp"

namespace roq {
namespace binance {
namespace json {

struct CancelOrderTemplate final {
  CancelRestrictions cancel_restrictions = {};
  CancelReplaceMode cancel_replace_mode = {};
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::CancelOrderTemplate> {
  constexpr auto parse(format_parse_context &context) { return std::begin(context); }
  auto format(roq::binance::json::CancelOrderTemplate const &value, format_context &context) const {
    using namespace std::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(cancel_restrictions={}, )"
        R"(cancel_replace_mode={})"
        R"(}})"sv,
        value.cancel_restrictions,
        value.cancel_replace_mode);
  }
};
