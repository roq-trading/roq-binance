/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <cstdint>

namespace roq {
namespace binance {

enum class RestState : uint8_t {
  UNDEFINED = 0,
  EXCHANGE_INFO,
  DONE,
};

}  // namespace binance
}  // namespace roq
