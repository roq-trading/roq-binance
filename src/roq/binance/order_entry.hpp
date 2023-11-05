/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/io/context.hpp"

#include "roq/api.hpp"

#include "roq/metrics/writer.hpp"

#include "roq/oms/order.hpp"

namespace roq {
namespace binance {

struct OrderEntry {
  struct ListenKeyUpdate final {
    std::string_view account;
    std::string_view listen_key;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(
        Trace<TradeUpdate> const &, bool is_last, uint8_t user_id, std::string_view const &request_id) = 0;
    virtual void operator()(Trace<FundsUpdate> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(ListenKeyUpdate const &) = 0;
  };

  OrderEntry() = default;

  virtual ~OrderEntry() = default;

  OrderEntry(OrderEntry &&) = delete;
  OrderEntry(OrderEntry const &) = delete;

  virtual bool ready() const = 0;

  virtual void operator()(Event<Start> const &) = 0;
  virtual void operator()(Event<Stop> const &) = 0;
  virtual void operator()(Event<Timer> const &) = 0;

  virtual void operator()(metrics::Writer &) = 0;

  virtual void operator()(Event<Disconnected> const &) = 0;

  virtual uint16_t operator()(Event<CreateOrder> const &, oms::Order const &, std::string_view const &request_id) = 0;
  virtual uint16_t operator()(
      Event<ModifyOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) = 0;
  virtual uint16_t operator()(
      Event<CancelOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) = 0;
  virtual uint16_t operator()(Event<CancelAllOrders> const &, std::string_view const &request_id) = 0;
};

}  // namespace binance
}  // namespace roq
