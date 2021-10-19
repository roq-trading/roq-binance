/* Copyright (c) 2017-2021, Hans Erik Thrane */

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
    virtual void operator()(const server::Trace<Error> &, int32_t id) = 0;
    virtual void operator()(const server::Trace<Result> &, int32_t id) = 0;
    // update
    virtual void operator()(const server::Trace<AggTrade> &) = 0;
    virtual void operator()(const server::Trace<Trade> &) = 0;
    virtual void operator()(const server::Trace<MiniTicker> &) = 0;
    virtual void operator()(const server::Trace<BookTicker> &) = 0;
    virtual void operator()(const server::Trace<Depth> &, const std::string_view &symbol) = 0;
    virtual void operator()(const server::Trace<DepthUpdate> &) = 0;
  };

  static void dispatch(
      Handler &handler,
      const std::string_view &message,
      core::json::Buffer &buffer,
      const server::TraceInfo &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
