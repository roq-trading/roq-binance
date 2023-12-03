/* Copyright (c) 2017-2024, Hans Erik Thrane */

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
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::binance::json::CancelOrderTemplate const &value, Context &context) const {
    using namespace std::literals;
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(cancel_restrictions={}, )"
        R"(cancel_replace_mode={})"
        R"(}})"_cf,
        value.cancel_restrictions,
        value.cancel_replace_mode);
  }
};
