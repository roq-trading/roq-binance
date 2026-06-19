/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>
#include <vector>

#include "roq/api.hpp"

#include "roq/server.hpp"

#include "roq/utils/container.hpp"

#include "roq/market/mbp/sequencer.hpp"

#include "roq/core/symbols.hpp"
#include "roq/core/timer_queue.hpp"

#include "roq/core/limit/rate_limiter.hpp"

#include "roq/binance/gateway/api.hpp"
#include "roq/binance/gateway/config.hpp"
#include "roq/binance/gateway/settings.hpp"

#include "roq/binance/protocol/json/cancel_order_template.hpp"
#include "roq/binance/protocol/json/create_order_template.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct Shared final {
  Shared(server::Dispatcher &, Settings const &, Config const &);

  Shared(Shared const &) = delete;

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
    explicit Instrument(Settings const &);

    int64_t tob_last_update_id = {};
    int64_t mbp_last_update_id = {};
    market::mbp::Sequencer sequencer;

    bool tob_update(int64_t update_id) {
      if (update_id < tob_last_update_id) {
        return false;
      }
      tob_last_update_id = update_id;
      return true;
    }

    bool mbp_update(int64_t update_id) {
      if (update_id < mbp_last_update_id) {
        return false;
      }
      mbp_last_update_id = update_id;
      return true;
    }
  };

  Instrument &get_instrument(std::string_view const &symbol);

 private:
  utils::unordered_map<std::string, Instrument> instruments_;

 public:
  server::Dispatcher &dispatcher;

  Settings const &settings;
  API const api;

  core::limit::RateLimiter rate_limiter;

  core::Symbols symbols;
  utils::unordered_set<std::string> all_symbols;

  core::TimerQueue<std::string> depth_request_queue;

 private:
  utils::unordered_map<std::string, protocol::json::CreateOrderTemplate> const &create_order_templates;
  utils::unordered_map<std::string, protocol::json::CancelOrderTemplate> const &cancel_order_templates;

 public:
  protocol::json::CreateOrderTemplate const &get_create_order_template(std::string_view const &name);
  protocol::json::CancelOrderTemplate const &get_cancel_order_template(std::string_view const &name);

  struct {
    uint32_t request_weight_1m = {};
    uint32_t create_order_10s = {};
    uint32_t create_order_1d = {};
  } limits;
  std::vector<RateLimit> rate_limits;

  bool const allow_unknown_event_types;

  std::vector<MBPUpdate> final_bids, final_asks;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
