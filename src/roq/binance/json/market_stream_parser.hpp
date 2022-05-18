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
    virtual void operator()(Trace<Error const> const &, int32_t id) = 0;
    virtual void operator()(Trace<Result const> const &, int32_t id) = 0;
    // update
    virtual void operator()(Trace<AggTrade const> const &) = 0;
    virtual void operator()(Trace<Trade const> const &) = 0;
    virtual void operator()(Trace<MiniTicker const> const &) = 0;
    virtual void operator()(Trace<BookTicker const> const &) = 0;
    virtual void operator()(Trace<Depth const> const &, std::string_view const &symbol) = 0;
    virtual void operator()(Trace<DepthUpdate const> const &) = 0;
  };

  static void dispatch(Handler &, std::string_view const &message, core::json::Buffer &, TraceInfo const &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
