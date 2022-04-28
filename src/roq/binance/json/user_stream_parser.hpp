/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.hpp"

#include "roq/server.hpp"

#include "roq/binance/json/event_type.hpp"

#include "roq/binance/json/balance_update.hpp"
#include "roq/binance/json/execution_report.hpp"
#include "roq/binance/json/list_status.hpp"
#include "roq/binance/json/outbound_account_position.hpp"

namespace roq {
namespace binance {
namespace json {

struct UserStreamParser final {
  struct Handler {
    virtual void operator()(const Trace<OutboundAccountPosition const> &) = 0;
    virtual void operator()(const Trace<BalanceUpdate const> &) = 0;
    virtual void operator()(const Trace<ExecutionReport const> &) = 0;
    virtual void operator()(const Trace<ListStatus const> &) = 0;
  };

  static void dispatch(
      Handler &, const std::string_view &message, core::json::Buffer &, const TraceInfo &);

 private:
  static bool try_dispatch(
      UserStreamParser::Handler &,
      const std::string_view &message,
      core::json::Buffer &,
      EventType,
      const TraceInfo &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
