/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.h"

#include "roq/server.h"

#include "roq/binance/json/error.h"
#include "roq/binance/json/result.h"

#include "roq/binance/json/agg_trade.h"
#include "roq/binance/json/book_ticker.h"
#include "roq/binance/json/depth.h"
#include "roq/binance/json/depth_update.h"
#include "roq/binance/json/mini_ticker.h"
#include "roq/binance/json/trade.h"

namespace roq {
namespace binance {
namespace json {

struct MarketStreamParser final {
  struct Handler {
    // response

    virtual void operator()(int32_t, const Error&) = 0;
    virtual void operator()(int32_t, const Result&) = 0;

    // update

    virtual void operator()(
        const AggTrade&,
        const server::Trace&) = 0;
    virtual void operator()(
        const Trade&,
        const server::Trace&) = 0;

    virtual void operator()(
        const MiniTicker&,
        const server::Trace&) = 0;
    virtual void operator()(
        const BookTicker&,
        const server::Trace&) = 0;

    virtual void operator()(
        const std::string_view& symbol,
        const Depth& depth,
        const server::Trace&) = 0;

    virtual void operator()(
        const std::string_view& symbol,
        const DepthUpdate& depth_update,
        const server::Trace&) = 0;
  };

  static void dispatch(
      Handler& handler,
      const std::string_view& message,
      core::json::Buffer& buffer,
      const server::Trace&);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
