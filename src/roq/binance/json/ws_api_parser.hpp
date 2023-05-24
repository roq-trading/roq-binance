/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/api.hpp"

#include "roq/binance/json/ws_api_request.hpp"

#include "roq/binance/json/error.hpp"

#include "roq/binance/json/listen_key.hpp"

#include "roq/binance/json/account.hpp"
#include "roq/binance/json/cancel_all_open_orders.hpp"
#include "roq/binance/json/cancel_order.hpp"
#include "roq/binance/json/cancel_replace_order.hpp"
#include "roq/binance/json/cancel_replace_order_error.hpp"
#include "roq/binance/json/new_order.hpp"
#include "roq/binance/json/open_orders.hpp"

namespace roq {
namespace binance {
namespace json {

struct WSAPIParser final {
  struct Handler {
    virtual void operator()(Trace<Error> const &, WSAPIRequest const &, int32_t status) = 0;
    virtual void operator()(Trace<ListenKey> const &, WSAPIRequest const &, int32_t status) = 0;
    virtual void operator()(Trace<Account> const &, WSAPIRequest const &, int32_t status) = 0;
    virtual void operator()(Trace<OpenOrders> const &, WSAPIRequest const &, int32_t status) = 0;
    virtual void operator()(Trace<CancelAllOpenOrders> const &, WSAPIRequest const &, int32_t status) = 0;
    virtual void operator()(Trace<NewOrder> const &, WSAPIRequest const &, int32_t status) = 0;
    virtual void operator()(Trace<CancelOrder> const &, WSAPIRequest const &, int32_t status) = 0;
    virtual void operator()(Trace<CancelReplaceOrder> const &, WSAPIRequest const &, int32_t status) = 0;
    virtual void operator()(Trace<CancelReplaceOrderError> const &, WSAPIRequest const &, int32_t status) = 0;
  };

  static bool dispatch(Handler &, std::string_view const &message, std::span<std::byte> const &, TraceInfo const &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
