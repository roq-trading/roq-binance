/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/shared.h"

#include "roq/binance/flags.h"

namespace roq {
namespace binance {

Shared::Shared(server::Dispatcher &dispatcher)
    : bids(server::Flags::cache_mbp_max_depth()), asks(server::Flags::cache_mbp_max_depth()),
      final_bids(server::Flags::cache_mbp_max_depth()),
      final_asks(server::Flags::cache_mbp_max_depth()), dispatcher_(dispatcher) {
}

}  // namespace binance
}  // namespace roq
