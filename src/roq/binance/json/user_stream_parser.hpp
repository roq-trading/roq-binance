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
    virtual void operator()(const server::Trace<OutboundAccountPosition> &) = 0;
    virtual void operator()(const server::Trace<BalanceUpdate> &) = 0;
    virtual void operator()(const server::Trace<ExecutionReport> &) = 0;
    virtual void operator()(const server::Trace<ListStatus> &) = 0;
  };

  static void dispatch(
      Handler &handler,
      const std::string_view &message,
      core::json::Buffer &buffer,
      const server::TraceInfo &trace);

 private:
  static bool try_dispatch(
      UserStreamParser::Handler &handler,
      const std::string_view &message,
      core::json::Buffer &buffer,
      EventType event_type,
      const server::TraceInfo &trace);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
