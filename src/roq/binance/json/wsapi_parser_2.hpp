/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <span>
#include <string_view>

#include "roq/trace.hpp"
#include "roq/trace_info.hpp"

#include "roq/binance/json/wsapi_request.hpp"

#include "roq/binance/json/wsapi_account.hpp"
#include "roq/binance/json/wsapi_cancel_open_orders.hpp"
#include "roq/binance/json/wsapi_cancel_order.hpp"
#include "roq/binance/json/wsapi_cancel_replace_order.hpp"
#include "roq/binance/json/wsapi_listen_key.hpp"
#include "roq/binance/json/wsapi_open_orders.hpp"
#include "roq/binance/json/wsapi_order_place.hpp"
#include "roq/binance/json/wsapi_trades.hpp"

namespace roq {
namespace binance {
namespace json {

struct WSAPIParser2 final {
  struct Handler {
    virtual void operator()(Trace<WSAPIListenKey> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPIAccount> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPIOpenOrders> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPITrades> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPICancelOpenOrders> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPIOrderPlace> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPICancelOrder> const &, WSAPIRequest const &) = 0;
    virtual void operator()(Trace<WSAPICancelReplaceOrder> const &, WSAPIRequest const &) = 0;
  };

  static bool dispatch(Handler &, std::string_view const &message, std::span<std::byte> const &, TraceInfo const &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
