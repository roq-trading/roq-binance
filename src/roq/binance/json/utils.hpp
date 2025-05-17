/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <vector>

#include "roq/server/oms/order.hpp"

#include "roq/utils/patterns.hpp"

#include "roq/utils/charconv/from_chars.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/web/http/status.hpp"

#include "roq/binance/json/cancel_order_template.hpp"
#include "roq/binance/json/create_order_template.hpp"

namespace roq {
namespace binance {
namespace json {

template <typename T>
inline void update(T &result, core::json::Value const &value) {
  result = core::json::get<T>(value);
}

template <>
inline void update(std::chrono::milliseconds &result, core::json::Value const &value) {
  using result_type = std::remove_cvref_t<decltype(result)>;
  std::visit(
      utils::overloaded{
          [&](core::json::Null const &) { result = result_type{}; },
          [](bool) { throw std::bad_cast{}; },
          [&](int64_t val) { result = result_type{val}; },
          [&](double val) { result = result_type{static_cast<int64_t>(val)}; },
          [&](std::string_view const &val) { result = utils::charconv::from_chars<result_type>(val, utils::charconv::Format::DATETIME); },
          [](core::json::Object const &) { throw std::bad_cast{}; },
          [](core::json::Array const &) { throw std::bad_cast{}; },
      },
      value);
}

extern roq::Error guess_error(int32_t code);

// new

extern std::string_view my_trades(
    std::vector<char> &buffer, std::string_view const &symbol, std::chrono::nanoseconds lookback, uint32_t limit, std::chrono::milliseconds now);

// cancel-all

extern std::string_view cancel_all_open_orders(std::vector<char> &buffer, std::string_view const &symbol, std::chrono::milliseconds recv_window);

// new

extern std::string_view new_order(
    std::vector<char> &buffer,
    CreateOrder const &,
    server::oms::Order const &,
    std::string_view const &request_id,
    CreateOrderTemplate const &,
    std::chrono::milliseconds recv_window);

extern std::string_view new_order_ws_url(
    std::vector<char> &buffer,
    CreateOrder const &,
    server::oms::Order const &,
    std::string_view const &request_id,
    CreateOrderTemplate const &,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now);

extern std::string_view new_order_ws_json(
    std::vector<char> &buffer,
    CreateOrder const &,
    server::oms::Order const &,
    std::string_view const &request_id,
    CreateOrderTemplate const &,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now,
    std::string_view const &signature);

// cancel

extern std::string_view cancel_order(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    server::oms::Order const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    CancelOrderTemplate const &,
    std::chrono::milliseconds recv_window);

extern std::string_view cancel_order_ws_url(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    server::oms::Order const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    CancelOrderTemplate const &,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now);

extern std::string_view cancel_order_ws_json(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    server::oms::Order const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    CancelOrderTemplate const &,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now,
    std::string_view const &signature);

// cancel-replace

extern std::string_view cancel_replace_order(
    std::vector<char> &buffer,
    std::string_view const &cancel_request_id,
    std::string_view const &cancel_previous_request_id,
    std::string_view const &cancel_external_order_id,
    CreateOrder const &,
    server::oms::Order const &,
    std::string_view const &create_request_id,
    CancelOrderTemplate const &,
    CreateOrderTemplate const &,
    std::chrono::milliseconds recv_window,
    bool stop_on_failure);

extern std::string_view cancel_replace_order_ws_url(
    std::vector<char> &buffer,
    std::string_view const &cancel_request_id,
    std::string_view const &cancel_previous_request_id,
    std::string_view const &cancel_external_order_id,
    CreateOrder const &,
    server::oms::Order const &,
    std::string_view const &create_request_id,
    CancelOrderTemplate const &,
    CreateOrderTemplate const &,
    std::chrono::milliseconds recv_window,
    bool stop_on_failure,
    std::string_view const &api_key,
    std::chrono::milliseconds now);

extern std::string_view cancel_replace_order_ws_json(
    std::vector<char> &buffer,
    std::string_view const &cancel_request_id,
    std::string_view const &cancel_previous_request_id,
    std::string_view const &cancel_external_order_id,
    CreateOrder const &,
    server::oms::Order const &,
    std::string_view const &create_request_id,
    CancelOrderTemplate const &,
    CreateOrderTemplate const &,
    std::chrono::milliseconds recv_window,
    bool stop_on_failure,
    std::string_view const &api_key,
    std::chrono::milliseconds now,
    std::string_view const &signature);

}  // namespace json
}  // namespace binance
}  // namespace roq
