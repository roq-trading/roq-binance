/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/margin_mode.hpp"

namespace roq {
namespace binance {
namespace gateway {

struct ListenKeyUpdate final {
  std::string_view account;
  MarginMode margin_mode = {};
  bool is_margin = {};
  std::string_view listen_key;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
