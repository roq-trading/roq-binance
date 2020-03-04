/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.h"

namespace roq {
namespace binance {
namespace json {

struct Parser final {
  struct Handler {
    // virtual void operator()(
  };

  static void dispatch(
      Handler& handler,
      const std::string_view& message,
      core::json::Buffer& buffer);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
