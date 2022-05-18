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
    virtual void operator()(Trace<OutboundAccountPosition const> const &) = 0;
    virtual void operator()(Trace<BalanceUpdate const> const &) = 0;
    virtual void operator()(Trace<ExecutionReport const> const &) = 0;
    virtual void operator()(Trace<ListStatus const> const &) = 0;
  };

  static void dispatch(Handler &, std::string_view const &message, core::json::Buffer &, TraceInfo const &);

 private:
  static bool try_dispatch(
      UserStreamParser::Handler &, std::string_view const &message, core::json::Buffer &, EventType, TraceInfo const &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
