/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/trace_info.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/binance/protocol/json/error.hpp"
#include "roq/binance/protocol/json/result.hpp"

#include "roq/binance/protocol/json/server_shutdown.hpp"

#include "roq/binance/protocol/json/agg_trade.hpp"
#include "roq/binance/protocol/json/book_ticker.hpp"
#include "roq/binance/protocol/json/depth.hpp"
#include "roq/binance/protocol/json/depth_update.hpp"
#include "roq/binance/protocol/json/mini_ticker.hpp"
#include "roq/binance/protocol/json/trade.hpp"

namespace roq {
namespace binance {
namespace protocol {
namespace json {

struct MarketStreamParser final {
  struct Handler {
    virtual void operator()(Trace<Error> const &, int64_t id) = 0;
    virtual void operator()(Trace<Result> const &, int64_t id) = 0;

    virtual void operator()(Trace<ServerShutdown> const &) = 0;

    virtual void operator()(Trace<AggTrade> const &) = 0;
    virtual void operator()(Trace<Trade> const &) = 0;
    virtual void operator()(Trace<MiniTicker> const &) = 0;
    virtual void operator()(Trace<BookTicker> const &) = 0;
    virtual void operator()(Trace<Depth> const &, std::string_view const &symbol) = 0;
    virtual void operator()(Trace<DepthUpdate> const &) = 0;
  };

  static bool dispatch(Handler &, std::string_view const &message, core::json::BufferStack &, TraceInfo const &, bool allow_unknown_event_types);
};

}  // namespace json
}  // namespace protocol
}  // namespace binance
}  // namespace roq
