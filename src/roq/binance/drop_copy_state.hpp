/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <cstdint>

namespace roq {
namespace binance {

enum class DropCopyState : uint8_t {
  UNDEFINED = 0,
  ACCOUNT,
  ORDERS,
  TRADES,
  DONE,
};

}  // namespace binance
}  // namespace roq
