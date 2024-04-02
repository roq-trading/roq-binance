/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include "roq/api.hpp"

#include "roq/metrics/writer.hpp"

namespace roq {
namespace binance {

struct DropCopy {
  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(
        Trace<TradeUpdate> const &, bool is_last, uint8_t user_id, std::string_view const &request_id) = 0;
    virtual void operator()(Trace<FundsUpdate> const &, bool is_last) = 0;
  };

  DropCopy() = default;

  DropCopy(DropCopy &&) = delete;
  DropCopy(DropCopy const &) = delete;

  virtual ~DropCopy() {}

  virtual bool ready() const = 0;

  virtual void operator()(Event<Start> const &) = 0;
  virtual void operator()(Event<Stop> const &) = 0;
  virtual void operator()(Event<Timer> const &) = 0;

  virtual void operator()(metrics::Writer &) = 0;
};

}  // namespace binance
}  // namespace roq
