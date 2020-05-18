/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.h"

#include "roq/binance/json/trade.h"
#include "roq/binance/json/ticker.h"
#include "roq/binance/json/depth_update.h"
/*
#include "roq/binance/json/account_update.h"
#include "roq/binance/json/balance_update.h"
#include "roq/binance/json/execution_report.h"
*/

namespace roq {
namespace binance {
namespace json {

struct Parser final {
  struct Handler {
    virtual void operator()(const Trade&) = 0;
    virtual void operator()(const Ticker&) = 0;
    virtual void operator()(const DepthUpdate&) = 0;
    /*
    virtual void operator()(const AccountUpdate&) = 0;
    virtual void operator()(const BalanceUpdate&) = 0;
    virtual void operator()(const ExecutionReport&) = 0;
    */
  };

  static void dispatch(
      Handler& handler,
      const std::string_view& message,
      core::json::Buffer& buffer);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
