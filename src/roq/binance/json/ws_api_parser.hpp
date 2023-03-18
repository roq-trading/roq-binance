/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/api.hpp"

#include "roq/core/json/buffer.hpp"

#include "roq/binance/json/listen_key.hpp"

namespace roq {
namespace binance {
namespace json {

struct WSAPIParser final {
  struct Handler {
    virtual void operator()(Trace<ListenKey> const &) = 0;
  };

  static bool dispatch(Handler &, std::string_view const &message, core::json::Buffer &, TraceInfo const &);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
