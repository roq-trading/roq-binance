/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <magic_enum/magic_enum_format.hpp>

namespace roq {
namespace binance {
namespace json {

enum class SideEffectType {
  UNDEFINED,
  NO_SIDE_EFFECT,
  MARGIN_BUY,
  AUTO_REPAY,
  AUTO_BORROW_REPAY,
};

}  // namespace json
}  // namespace binance
}  // namespace roq
