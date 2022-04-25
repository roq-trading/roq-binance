/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.hpp"

#include "roq/server.hpp"

#include "roq/binance/json/error.hpp"
#include "roq/binance/json/result.hpp"

#include "roq/binance/json/agg_trade.hpp"
#include "roq/binance/json/book_ticker.hpp"
#include "roq/binance/json/depth.hpp"
#include "roq/binance/json/depth_update.hpp"
#include "roq/binance/json/mini_ticker.hpp"
#include "roq/binance/json/trade.hpp"

namespace roq {
namespace binance {
namespace json {

struct MarketStreamParser final {
  struct Handler {
    // response
    virtual void operator()(const Trace<Error const> &, int32_t id) = 0;
    virtual void operator()(const Trace<Result const> &, int32_t id) = 0;
    // update
    virtual void operator()(const Trace<AggTrade const> &) = 0;
    virtual void operator()(const Trace<Trade const> &) = 0;
    virtual void operator()(const Trace<MiniTicker const> &) = 0;
    virtual void operator()(const Trace<BookTicker const> &) = 0;
    virtual void operator()(const Trace<Depth const> &, const std::string_view &symbol) = 0;
    virtual void operator()(const Trace<DepthUpdate const> &) = 0;
  };

  static void dispatch(
      Handler &handler,
      const std::string_view &message,
      core::json::Buffer &buffer,
      const TraceInfo &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
