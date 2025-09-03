/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <span>
#include <string_view>

#include "roq/trace_info.hpp"

#include "roq/core/json/buffer_stack.hpp"

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
    virtual void operator()(Trace<OutboundAccountPosition> const &) = 0;
    virtual void operator()(Trace<BalanceUpdate> const &) = 0;
    virtual void operator()(Trace<ExecutionReport> const &) = 0;
    virtual void operator()(Trace<ListStatus> const &) = 0;
  };

  // traditional
  static bool dispatch(Handler &, std::string_view const &message, core::json::BufferStack &, TraceInfo const &);

  // papi
  static bool dispatch_papi(Handler &, std::string_view const &message, core::json::BufferStack &, TraceInfo const &);

 private:
  static bool try_dispatch(UserStreamParser::Handler &, std::string_view const &message, core::json::BufferStack &, EventType, TraceInfo const &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
