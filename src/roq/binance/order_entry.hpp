/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/api.hpp"

#include "roq/metrics/writer.hpp"

#include "roq/server/oms/order.hpp"

namespace roq {
namespace binance {

struct OrderEntry {
  struct ListenKeyUpdate final {
    std::string_view account;
    MarginMode margin_mode = {};
    bool is_margin = {};
    std::string_view listen_key;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<RateLimitsUpdate> const &) = 0;
    virtual void operator()(Trace<TradeUpdate> const &, bool is_last, uint8_t user_id, std::string_view const &request_id) = 0;
    virtual void operator()(Trace<FundsUpdate> const &, bool is_last) = 0;
  };

  OrderEntry() = default;

  OrderEntry(OrderEntry const &) = delete;

  virtual ~OrderEntry() = default;

  // virtual bool ready() const = 0;

  virtual void operator()(Event<Start> const &) = 0;
  virtual void operator()(Event<Stop> const &) = 0;
  virtual void operator()(Event<Timer> const &) = 0;

  virtual void operator()(metrics::Writer &) const = 0;

  // virtual void operator()(Event<Disconnected> const &) = 0;

  virtual uint16_t operator()(Event<CreateOrder> const &, server::oms::Order const &, std::string_view const &request_id) = 0;
  virtual uint16_t operator()(
      Event<ModifyOrder> const &, server::oms::Order const &, std::string_view const &request_id, std::string_view const &previous_request_id) = 0;
  virtual uint16_t operator()(
      Event<CancelOrder> const &, server::oms::Order const &, std::string_view const &request_id, std::string_view const &previous_request_id) = 0;
  virtual uint16_t operator()(Event<CancelAllOrders> const &, std::string_view const &request_id) = 0;
};

}  // namespace binance
}  // namespace roq
