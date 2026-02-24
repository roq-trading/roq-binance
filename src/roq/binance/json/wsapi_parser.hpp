/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/trace.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/binance/json/wsapi_request.hpp"

#include "roq/binance/json/wsapi_session_logon.hpp"

#include "roq/binance/json/wsapi_event_stream_terminated.hpp"
#include "roq/binance/json/wsapi_user_data_stream_subscribe.hpp"

#include "roq/binance/json/wsapi_account.hpp"
#include "roq/binance/json/wsapi_open_orders.hpp"
#include "roq/binance/json/wsapi_trades.hpp"

#include "roq/binance/json/wsapi_cancel_open_orders.hpp"
#include "roq/binance/json/wsapi_cancel_order.hpp"
#include "roq/binance/json/wsapi_order_amend_keep_priority.hpp"
#include "roq/binance/json/wsapi_order_place.hpp"

#include "roq/binance/json/wsapi_balance_update.hpp"
#include "roq/binance/json/wsapi_execution_report.hpp"
#include "roq/binance/json/wsapi_outbound_account_position.hpp"

namespace roq {
namespace binance {
namespace json {

struct WSAPIParser final {
  struct Handler {
    virtual void operator()(Trace<WSAPISessionLogon> const &) = 0;
    //
    virtual void operator()(Trace<WSAPIUserDataStreamSubscribe> const &) = 0;
    virtual void operator()(Trace<WSAPIEventStreamTerminated> const &) = 0;
    //
    virtual void operator()(Trace<WSAPIAccount> const &) = 0;
    virtual void operator()(Trace<WSAPIOpenOrders> const &) = 0;
    virtual void operator()(Trace<WSAPITrades> const &) = 0;
    //
    virtual void operator()(Trace<WSAPIOrderPlace> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPIOrderAmendKeepPriority> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPICancelOrder> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPICancelOpenOrders> const &, WSAPIRequest const &) = 0;
    //
    virtual void operator()(Trace<WSAPIOutboundAccountPosition> const &) = 0;
    virtual void operator()(Trace<WSAPIBalanceUpdate> const &) = 0;
    virtual void operator()(Trace<WSAPIExecutionReport> const &) = 0;
  };

  static bool dispatch(Handler &, std::string_view const &message, core::json::BufferStack &, TraceInfo const &, bool allow_unknown_event_types = false);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
