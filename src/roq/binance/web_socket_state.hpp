/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <cstdint>

namespace roq {
namespace binance {

enum class WebSocketState : uint8_t {
  UNDEFINED = 0,
  SESSION_LOGON,
  USER_DATA_STREAM_SUBSCRIBE,
  ACCOUNT_STATUS,
  OPEN_ORDERS_STATUS,
  MY_TRADES,
  DONE,
};

}  // namespace binance
}  // namespace roq
