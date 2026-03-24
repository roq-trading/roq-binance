/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/json/encoder.hpp"

#include "roq/logging.hpp"

#include "roq/decimal.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/binance/json/map.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

// === HELPERS ===

namespace {
json::OrderType map_order_type(auto &order) {
  switch (order.order_type) {
    using enum roq::OrderType;
    case UNDEFINED:
      break;
    case MARKET:
      if (!std::isnan(order.stop_price)) {
        return json::OrderType::STOP_LOSS;
      }
      return json::OrderType::MARKET;
    case LIMIT:
      if (order.execution_instructions.has(ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE)) {
        return json::OrderType::LIMIT_MAKER;
      }
      if (!std::isnan(order.stop_price)) {
        return json::OrderType::STOP_LOSS_LIMIT;
      }
      return json::OrderType::LIMIT;
  }
  return map(order.order_type);
}

json::TimeInForce map_time_in_force(auto &create_order) {
  switch (create_order.order_type) {
    using enum roq::OrderType;
    case UNDEFINED:
      break;
    case MARKET:
      return {};
    case LIMIT:
      if (create_order.execution_instructions.has(ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE)) {
        return {};
      }
      break;
  }
  return map(create_order.time_in_force);
}

auto get_cancel_replace_mode(auto &cancel_order_template, bool stop_on_failure) {
  switch (cancel_order_template.cancel_replace_mode) {
    using enum CancelReplaceMode::type_t;
    case UNDEFINED_INTERNAL:
      break;
    case UNKNOWN_INTERNAL:
      log::fatal("Unexpected"sv);
      break;
    case STOP_ON_FAILURE:
    case ALLOW_FAILURE:
      return cancel_order_template.cancel_replace_mode.as_raw_text();
  }
  return stop_on_failure ? "STOP_ON_FAILURE"sv : "ALLOW_FAILURE"sv;
}
}  // namespace

// === IMPLEMENTATION ===

// wsapi

std::string_view Encoder::place_order_json(
    std::string &buffer,
    CreateOrder const &create_order,
    server::oms::Order const &,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window,
    std::chrono::milliseconds now_utc) {
  auto side = map(create_order.side).template get<Side>();
  auto type = map_order_type(create_order);
  auto time_in_force = map_time_in_force(create_order);
  buffer.clear();
  fmt::format_to(
      std::back_inserter(buffer),
      R"({{)"
      R"("newClientOrderId":"{}",)"sv,
      request_id);
  if (!std::isnan(create_order.price)) {
    fmt::format_to(std::back_inserter(buffer), R"("price":"{}",)"sv, Decimal{create_order.price, ref_data.price.precision});
  }
  fmt::format_to(
      std::back_inserter(buffer),
      R"("quantity":"{}",)"
      R"("recvWindow":{},)"sv,
      Decimal{create_order.quantity, ref_data.quantity.precision},
      recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{}) {
    fmt::format_to(std::back_inserter(buffer), R"("selfTradePreventionMode":"{}",)"sv, create_order_template.self_trade_prevention_mode.as_raw_text());
  }
  fmt::format_to(std::back_inserter(buffer), R"("side":"{}",)"sv, side.as_raw_text());
  if (!std::isnan(create_order.stop_price)) {
    fmt::format_to(std::back_inserter(buffer), R"("stopPrice":"{}",)"sv, Decimal{create_order.stop_price, ref_data.price.precision});
  }
  fmt::format_to(std::back_inserter(buffer), R"("symbol":"{}",)"sv, create_order.symbol);
  if (time_in_force != json::TimeInForce{}) {
    fmt::format_to(std::back_inserter(buffer), R"("timeInForce":"{}",)"sv, time_in_force.as_raw_text());
  }
  fmt::format_to(
      std::back_inserter(buffer),
      R"("timestamp":{},)"
      R"("type":"{}")"
      R"(}})"sv,
      now_utc.count(),
      type.as_raw_text());
  return buffer;
}

std::string_view Encoder::amend_order_keep_priority_json(
    std::string &buffer,
    roq::ModifyOrder const &modify_order,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    std::chrono::milliseconds recv_window,
    std::chrono::milliseconds now_utc) {
  if (!std::isnan(modify_order.price)) {
    throw server::oms::Rejected{Origin::GATEWAY, Error::INVALID_REQUEST_ARGS, "Price not allowed"sv};
  }
  buffer.clear();
  fmt::format_to(
      std::back_inserter(buffer),
      R"({{)"
      R"("newClientOrderId":"{}",)"sv,
      request_id);
  if (!std::empty(order.external_order_id)) {
    fmt::format_to(std::back_inserter(buffer), R"("orderId":{},)"sv, order.external_order_id);  // note! integer
  }
  fmt::format_to(
      std::back_inserter(buffer),
      R"("origClientOrderId":"{}",)"
      R"("newQty":"{}",)"
      R"("recvWindow":{},)"
      R"("symbol":"{}",)"
      R"("timestamp":{})"
      R"(}})"sv,
      previous_request_id,
      Decimal{modify_order.quantity, ref_data.quantity.precision},
      recv_window.count(),
      order.symbol,
      now_utc.count());
  return buffer;
}

std::string_view Encoder::cancel_order_json(
    std::string &buffer,
    roq::CancelOrder const &,
    server::oms::Order const &order,
    server::oms::RefData const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    CancelOrderTemplate const &cancel_order_template,
    std::chrono::milliseconds recv_window,
    std::chrono::milliseconds now_utc) {
  buffer.clear();
  fmt::format_to(std::back_inserter(buffer), R"({{)"sv);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{}) {
    fmt::format_to(std::back_inserter(buffer), R"("cancelRestrictions":"{}",)"sv, cancel_order_template.cancel_restrictions.as_raw_text());
  }
  fmt::format_to(std::back_inserter(buffer), R"("newClientOrderId":"{}",)"sv, request_id);
  if (!std::empty(order.external_order_id)) {
    fmt::format_to(std::back_inserter(buffer), R"("orderId":{},)"sv, order.external_order_id);  // note! integer
  }
  fmt::format_to(
      std::back_inserter(buffer),
      R"("origClientOrderId":"{}",)"
      R"("recvWindow":{},)"
      R"("symbol":"{}",)"
      R"("timestamp":{})"
      R"(}})"sv,
      previous_request_id,
      recv_window.count(),
      order.symbol,
      now_utc.count());
  return buffer;
}

// sapi+papi

std::string_view Encoder::my_trades_url(
    std::string &buffer, std::string_view const &symbol, std::chrono::nanoseconds lookback, uint32_t limit, std::chrono::milliseconds now_utc) {
  auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(now_utc - lookback);
  buffer.clear();
  fmt::format_to(
      std::back_inserter(buffer),
      R"(limit={})"
      R"(&startTime={})"
      R"(&symbol={})"sv,
      limit,
      start_time.count(),
      symbol);
  return buffer;
}

std::string_view Encoder::new_order_url(
    std::string &buffer,
    CreateOrder const &create_order,
    server::oms::Order const &,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window,
    std::chrono::milliseconds now_utc,
    SideEffectType side_effect_type) {
  auto side = map(create_order.side).template get<Side>();
  auto type = map_order_type(create_order);
  auto time_in_force = map_time_in_force(create_order);
  buffer.clear();
  if (create_order.margin_mode == MarginMode::ISOLATED) {
    fmt::format_to(std::back_inserter(buffer), "isIsolated=TRUE&"sv);
  }
  fmt::format_to(std::back_inserter(buffer), "newClientOrderId={}"sv, request_id);
  if (!std::isnan(create_order.price)) {
    fmt::format_to(std::back_inserter(buffer), "&price={}"sv, Decimal{create_order.price, ref_data.price.precision});
  }
  fmt::format_to(std::back_inserter(buffer), "&quantity={}"sv, Decimal{create_order.quantity, ref_data.quantity.precision});
  fmt::format_to(std::back_inserter(buffer), "&recvWindow={}"sv, recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{}) {
    fmt::format_to(std::back_inserter(buffer), "&selfTradePreventionMode={}"sv, create_order_template.self_trade_prevention_mode.as_raw_text());
  }
  fmt::format_to(std::back_inserter(buffer), "&side={}"sv, side.as_raw_text());
  if (side_effect_type != SideEffectType::UNDEFINED) {
    fmt::format_to(std::back_inserter(buffer), "&sideEffectType={}"sv, magic_enum::enum_name(side_effect_type));
  }
  if (!std::isnan(create_order.stop_price)) {
    fmt::format_to(std::back_inserter(buffer), "&stopPrice={}"sv, Decimal{create_order.stop_price, ref_data.price.precision});
  }
  fmt::format_to(std::back_inserter(buffer), "&symbol={}"sv, create_order.symbol);
  if (time_in_force != json::TimeInForce{}) {
    fmt::format_to(std::back_inserter(buffer), "&timeInForce={}"sv, time_in_force.as_raw_text());
  }
  fmt::format_to(
      std::back_inserter(buffer),
      "&timestamp={}"
      "&type={}"sv,
      now_utc.count(),
      type.as_raw_text());
  return buffer;
}

std::string_view Encoder::cancel_order_url(
    std::string &buffer,
    roq::CancelOrder const &,
    server::oms::Order const &order,
    server::oms::RefData const &,
    std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id,
    CancelOrderTemplate const &cancel_order_template,
    std::chrono::milliseconds recv_window,
    std::chrono::milliseconds now_utc) {
  buffer.clear();
  // assert(!std::empty(previous_request_id));
  if (order.margin_mode == MarginMode::ISOLATED) {
    fmt::format_to(std::back_inserter(buffer), "isIsolated=TRUE&"sv);
  }
  fmt::format_to(std::back_inserter(buffer), "newClientOrderId={}"sv, request_id);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{}) {
    fmt::format_to(std::back_inserter(buffer), "&cancelRestrictions={}"sv, cancel_order_template.cancel_restrictions.as_raw_text());
  }
  if (std::empty(order.external_order_id)) {
    fmt::format_to(std::back_inserter(buffer), "&origClientOrderId={}"sv, order.client_order_id);  // XXX FIXME TODO previous_request_id ???
  } else {
    fmt::format_to(std::back_inserter(buffer), "&orderId={}"sv, order.external_order_id);
  }
  fmt::format_to(
      std::back_inserter(buffer),
      "&recvWindow={}"
      "&symbol={}"
      "&timestamp={}"sv,
      recv_window.count(),
      order.symbol,
      now_utc.count());
  return buffer;
}

// cancel-all

std::string_view Encoder::cancel_all_open_orders_url(
    std::string &buffer, std::string_view const &symbol, MarginMode margin_mode, std::chrono::milliseconds recv_window, std::chrono::milliseconds now_utc) {
  buffer.clear();
  switch (margin_mode) {
    using enum MarginMode;
    case UNDEFINED:
      break;
    case ISOLATED:
      fmt::format_to(std::back_inserter(buffer), R"(isIsolated=TRUE&)"sv);
      break;
    case CROSS:
      fmt::format_to(std::back_inserter(buffer), R"(isIsolated=FALSE&)"sv);
      break;
    case PORTFOLIO:
      break;
  }
  fmt::format_to(
      std::back_inserter(buffer),
      R"(symbol={}&)"
      R"(recvWindow={}&)"
      R"(timestamp={})"sv,
      symbol,
      recv_window.count(),
      now_utc.count());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
