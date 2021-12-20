/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/shared.h"

#include "roq/binance/flags.h"

namespace roq {
namespace binance {

Shared::Shared(server::Dispatcher &dispatcher)
    : fills(server::Flags::cache_fills_max_depth()), bids(server::Flags::cache_mbp_max_depth()),
      asks(server::Flags::cache_mbp_max_depth()), final_bids(server::Flags::cache_mbp_max_depth()),
      final_asks(server::Flags::cache_mbp_max_depth()), dispatcher_(dispatcher),
      rate_limiter(Flags::request_limit(), Flags::request_limit_interval()),
      symbols(Flags::ws_max_subscriptions_per_stream()),
      depth_request_queue(Flags::ws_mbp_request_delay()) {
}

}  // namespace binance
}  // namespace roq
