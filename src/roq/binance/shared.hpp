/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "roq/api.hpp"
#include "roq/server.hpp"

#include "roq/core/symbols.hpp"
#include "roq/core/timer_queue.hpp"

#include "roq/core/limit/rate_limiter.hpp"

#include "roq/core/mbp/sequencer.hpp"

namespace roq {
namespace binance {

struct Shared final {
  explicit Shared(server::Dispatcher &);

  Shared(Shared &&) = default;
  Shared(Shared const &) = delete;

  auto discard_symbol(std::string_view const &name) const { return dispatcher_.discard_symbol(name); }

  auto get_symbol_id(std::string_view const &exchange, std::string_view const &symbol) {
    return dispatcher_.get_symbol_id(exchange, symbol);
  }
  auto get_exchange_symbol(uint32_t symbol_id) { return dispatcher_.get_exchange_symbol(symbol_id); }

  template <typename... Args>
  auto update_order(Args &&...args) {
    return dispatcher_.update_order(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto operator()(Args &&...args) {
    return dispatcher_(std::forward<Args>(args)...);
  }

 private:
  struct {
    std::vector<MBPUpdate> bids, asks;
    auto &clear() {
      bids.clear();
      asks.clear();
      return *this;
    }
    bool empty() const { return std::empty(bids) && std::empty(asks); }
  } mbp;

 public:
  auto &get_mbp() { return mbp.clear(); }

  struct Instrument final {
    int64_t tob_last_update_id = {};
    int64_t mbp_last_update_id = {};
    core::mbp::Sequencer sequencer;

    bool tob_update(int64_t update_id) {
      if (update_id < tob_last_update_id)
        return false;
      tob_last_update_id = update_id;
      return true;
    }

    bool mbp_update(int64_t update_id) {
      if (update_id < mbp_last_update_id)
        return false;
      mbp_last_update_id = update_id;
      return true;
    }
  };
  absl::flat_hash_map<Symbol, Instrument> instruments;

 private:
  server::Dispatcher &dispatcher_;

 public:
  core::limit::RateLimiter rate_limiter;
  core::Symbols symbols;
  core::TimerQueue depth_request_queue;
};

}  // namespace binance
}  // namespace roq
