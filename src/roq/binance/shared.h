/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <chrono>
#include <string>
#include <utility>

#include "roq/api.h"
#include "roq/server.h"

#include "roq/core/memory.h"
#include "roq/core/symbols.h"
#include "roq/core/timer_queue.h"

#include "roq/core/limit/rate_limiter.h"

#include "roq/core/market/mbp_sequencer.h"

namespace roq {
namespace binance {

struct Shared final {
  explicit Shared(server::Dispatcher &);

  Shared(Shared &&) = default;
  Shared(const Shared &) = delete;

  auto discard_symbol(const std::string_view &name) const {
    return dispatcher_.discard_symbol(name);
  }

  template <typename... Args>
  auto update_order(Args &&...args) {
    return dispatcher_.update_order(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto create_order(Args &&...args) {
    return dispatcher_.create_order(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto operator()(Args &&...args) {
    return dispatcher_(std::forward<Args>(args)...);
  }

 public:
  core::page_aligned_vector<Fill> fills;
  core::page_aligned_vector<MBPUpdate> bids, asks, final_bids, final_asks;

  absl::flat_hash_map<std::string, core::market::MBP_Sequencer> mbp_collector;

 private:
  server::Dispatcher &dispatcher_;

 public:
  core::limit::RateLimiter rate_limiter;
  core::Symbols symbols;
  core::TimerQueue depth_request_queue;
};

}  // namespace binance
}  // namespace roq
