/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "roq/cancel_all_orders.hpp"
#include "roq/cancel_order.hpp"
#include "roq/create_order.hpp"
#include "roq/modify_order.hpp"

#include "roq/server/oms/order.hpp"

#include "roq/web/http/status.hpp"

#include "roq/binance/json/cancel_order_template.hpp"
#include "roq/binance/json/create_order_template.hpp"
#include "roq/binance/json/side_effect_type.hpp"

namespace roq {
namespace binance {
namespace json {

struct Encoder final {
  // new

  static std::string_view new_order_ws_json(
      std::vector<char> &buffer,
      CreateOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      CreateOrderTemplate const &,
      std::chrono::milliseconds recv_window,
      std::chrono::milliseconds now);

  // cancel

  static std::string_view cancel_order_ws_json(
      std::vector<char> &buffer,
      roq::CancelOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id,
      CancelOrderTemplate const &,
      std::chrono::milliseconds recv_window,
      std::chrono::milliseconds now);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
