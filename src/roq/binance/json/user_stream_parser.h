/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.h"

#include "roq/binance/json/event_type.h"

#include "roq/binance/json/balance_update.h"
#include "roq/binance/json/execution_report.h"
#include "roq/binance/json/outbound_account_info.h"
#include "roq/binance/json/outbound_account_position.h"

namespace roq {
namespace binance {
namespace json {

struct UserStreamParser final {
  struct Handler {
    virtual void operator()(const OutboundAccountInfo&) = 0;
    virtual void operator()(const OutboundAccountPosition&) = 0;
    virtual void operator()(const BalanceUpdate&) = 0;
    virtual void operator()(const ExecutionReport&) = 0;
  };

  static void dispatch(
      Handler& handler,
      const std::string_view& message,
      core::json::Buffer& buffer);

 private:
  static bool try_dispatch(
      UserStreamParser::Handler& handler,
      const std::string_view& message,
      core::json::Buffer& buffer,
      EventType event_type);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
