/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/trace_info.hpp"

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
    virtual void operator()(Trace<Error> const &, int64_t id) = 0;
    virtual void operator()(Trace<Result> const &, int64_t id) = 0;
    // update
    virtual void operator()(Trace<AggTrade> const &) = 0;
    virtual void operator()(Trace<Trade> const &) = 0;
    virtual void operator()(Trace<MiniTicker> const &) = 0;
    virtual void operator()(Trace<BookTicker> const &) = 0;
    virtual void operator()(Trace<Depth> const &, std::string_view const &symbol) = 0;
    virtual void operator()(Trace<DepthUpdate> const &) = 0;
  };

  static bool dispatch(Handler &, std::string_view const &message, std::span<std::byte> const &, TraceInfo const &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
