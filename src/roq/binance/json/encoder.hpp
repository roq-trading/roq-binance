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
  // wsapi

  static std::string_view wsapi_place_order(
      std::vector<char> &buffer,
      CreateOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      CreateOrderTemplate const &,
      std::chrono::milliseconds recv_window,
      std::chrono::milliseconds now);

  static std::string_view wsapi_amend_order_keep_priority(
      std::vector<char> &buffer,
      ModifyOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id,
      std::chrono::milliseconds recv_window,
      std::chrono::milliseconds now);

  static std::string_view wsapi_cancel_order(
      std::vector<char> &buffer,
      roq::CancelOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id,
      CancelOrderTemplate const &,
      std::chrono::milliseconds recv_window,
      std::chrono::milliseconds now);

  // sapi+papi

  static std::string_view my_trades(
      std::vector<char> &buffer, std::string_view const &symbol, std::chrono::nanoseconds lookback, uint32_t limit, std::chrono::milliseconds now);

  static std::string_view new_order(
      std::vector<char> &buffer,
      CreateOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      CreateOrderTemplate const &,
      std::chrono::milliseconds recv_window,
      SideEffectType = {});

  static std::string_view cancel_order(
      std::vector<char> &buffer,
      roq::CancelOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id,
      CancelOrderTemplate const &,
      std::chrono::milliseconds recv_window);

  static std::string_view cancel_all_open_orders(std::vector<char> &buffer, std::string_view const &symbol, MarginMode, std::chrono::milliseconds recv_window);
};

}  // namespace json
}  // namespace binance
}  // namespace roq
