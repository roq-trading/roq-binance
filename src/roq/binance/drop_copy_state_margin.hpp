/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <cstdint>

namespace roq {
namespace binance {

enum class DropCopyStateMargin : uint8_t {
  UNDEFINED = 0,
  SESSION_LOGON,
  USER_DATA_STREAM_SUBSCRIBE,
  ACCOUNT,
  ORDERS,
  DONE,
};

}  // namespace binance
}  // namespace roq
