/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/json/encoder.hpp"

#include "roq/logging.hpp"

#include "roq/decimal.hpp"

#include "roq/utils/text/writer.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/binance/json/map.hpp"

#define WS_USE_FMT

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
    std::vector<char> &buffer,
    CreateOrder const &create_order,
    server::oms::Order const &order,
    std::string_view const &request_id,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window,
    std::chrono::milliseconds now) {
  auto side = map(create_order.side).template get<Side>();
  auto type = map_order_type(create_order);
  auto time_in_force = map_time_in_force(create_order);
#if defined(WS_USE_FMT)
  buffer.clear();
  fmt::format_to(
      std::back_inserter(buffer),
      R"({{)"
      R"("newClientOrderId":"{}",)"sv,
      request_id);
  if (!std::isnan(create_order.price)) {
    fmt::format_to(std::back_inserter(buffer), R"("price":"{}",)"sv, Decimal{create_order.price, order.price_precision.precision});
  }
  fmt::format_to(
      std::back_inserter(buffer),
      R"("quantity":"{}",)"
      R"("recvWindow":{},)"sv,
      Decimal{create_order.quantity, order.quantity_precision.precision},
      recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{}) {
    fmt::format_to(std::back_inserter(buffer), R"("selfTradePreventionMode":"{}",)"sv, create_order_template.self_trade_prevention_mode.as_raw_text());
  }
  fmt::format_to(std::back_inserter(buffer), R"("side":"{}",)"sv, side.as_raw_text());
  if (!std::isnan(create_order.stop_price)) {
    fmt::format_to(std::back_inserter(buffer), R"("stopPrice":"{}",)"sv, Decimal{create_order.stop_price, order.price_precision.precision});
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
      now.count(),
      type.as_raw_text());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
#else
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  writer.write("{"sv);
  writer.write(R"("newClientOrderId":")"sv).write(request_id).write(R"(")"sv);
  if (!std::isnan(create_order.price)) {
    writer.write(R"(,"price":")"sv).write(Decimal{create_order.price, order.price_precision.precision}).write(R"(")"sv);
  }
  writer.write(R"(,"quantity":")"sv).write(Decimal{create_order.quantity, order.quantity_precision.precision}).write(R"(")"sv);
  writer.write(R"(,"recvWindow":)"sv).write(recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{}) {
    writer.write(R"(,"selfTradePreventionMode":")"sv).write(create_order_template.self_trade_prevention_mode.as_raw_text()).write(R"(")"sv);
  }
  writer.write(R"(,"side":")"sv).write(side.as_raw_text()).write(R"(")"sv);
  if (!std::isnan(create_order.stop_price)) {
    writer.write(R"(,"stopPrice":")"sv).write(Decimal{create_order.stop_price, order.price_precision.precision}).write(R"(")"sv);
  }
  writer.write(R"(,"symbol":")"sv).write(create_order.symbol).write(R"(")"sv);
  if (time_in_force != json::TimeInForce{}) {
    writer.write(R"(,"timeInForce":")"sv).write(time_in_force.as_raw_text()).write(R"(")"sv);
  }
  writer.write(R"(,"timestamp":)"sv).write(now.count());
  writer.write(R"(,"type":")"sv).write(type.as_raw_text()).write(R"(")"sv);
  writer.write("}"sv);
  return writer.finish();
#endif
}

std::string_view Encoder::amend_order_keep_priority_json(
    std::vector<char> &buffer,
    roq::ModifyOrder const &modify_order,
    server::oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    std::chrono::milliseconds recv_window,
    std::chrono::milliseconds now) {
  if (!std::isnan(modify_order.price)) {
    throw server::oms::Rejected{Origin::GATEWAY, Error::INVALID_REQUEST_ARGS, "Price not allowed"sv};
  }
#if defined(WS_USE_FMT)
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
      Decimal{modify_order.quantity, order.quantity_precision.precision},
      recv_window.count(),
      order.symbol,
      now.count());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
#else
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  writer.write("{"sv);
  writer.write(R"("newClientOrderId":")"sv).write(request_id).write(R"(")"sv);
  if (!std::empty(order.external_order_id)) {
    writer.write(R"(,"orderId":)"sv).write(order.external_order_id);  // note! integer
  }
  writer.write(R"(,"origClientOrderId":")"sv).write(previous_request_id).write(R"(")"sv);
  writer.write(R"(,"newQty":")"sv).write(Decimal{modify_order.quantity, order.quantity_precision.precision}).write(R"(")"sv);
  writer.write(R"(,"recvWindow":)"sv).write(recv_window.count());
  writer.write(R"(,"symbol":")"sv).write(order.symbol).write(R"(")"sv);
  writer.write(R"(,"timestamp":)"sv).write(now.count());
  writer.write("}"sv);
  return writer.finish();
#endif
}

std::string_view Encoder::cancel_order_json(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    server::oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    CancelOrderTemplate const &cancel_order_template,
    std::chrono::milliseconds recv_window,
    std::chrono::milliseconds now) {
#if defined(WS_USE_FMT)
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
      now.count());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
#else
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  writer.write("{"sv);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{}) {
    writer.write(R"("cancelRestrictions":")"sv).write(cancel_order_template.cancel_restrictions.as_raw_text()).write(R"(",)"sv);
  }
  writer.write(R"("newClientOrderId":")"sv).write(request_id).write(R"(")"sv);
  if (!std::empty(order.external_order_id)) {
    writer.write(R"(,"orderId":)"sv).write(order.external_order_id);  // note! integer
  }
  writer.write(R"(,"origClientOrderId":")"sv).write(previous_request_id).write(R"(")"sv);
  writer.write(R"(,"recvWindow":)"sv).write(recv_window.count());
  writer.write(R"(,"symbol":")"sv).write(order.symbol).write(R"(")"sv);
  writer.write(R"(,"timestamp":)"sv).write(now.count());
  writer.write("}"sv);
  return writer.finish();
#endif
}

// sapi+papi

std::string_view Encoder::my_trades_url(
    std::vector<char> &buffer, std::string_view const &symbol, std::chrono::nanoseconds lookback, uint32_t limit, std::chrono::milliseconds now) {
  auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - lookback);
#if defined(WS_USE_FMT)
  buffer.clear();
  fmt::format_to(
      std::back_inserter(buffer),
      R"(limit={})"
      R"(&startTime={})"
      R"(&symbol={})"sv,
      limit,
      start_time.count(),
      symbol);
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
#else
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  writer.write("limit="sv).write(limit);
  writer.write("&startTime="sv).write(start_time.count());
  writer.write("&symbol="sv).write(symbol);
  return writer.finish();
#endif
}

std::string_view Encoder::new_order_url(
    std::vector<char> &buffer,
    CreateOrder const &create_order,
    server::oms::Order const &order,
    std::string_view const &request_id,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window,
    SideEffectType side_effect_type) {
  auto side = map(create_order.side).template get<Side>();
  auto type = map_order_type(create_order);
  auto time_in_force = map_time_in_force(create_order);
#if defined(WS_USE_FMT)
  buffer.clear();
  if (create_order.margin_mode == MarginMode::ISOLATED) {
    fmt::format_to(std::back_inserter(buffer), "isIsolated=TRUE&"sv);
  }
  fmt::format_to(std::back_inserter(buffer), "newClientOrderId={}"sv, request_id);
  if (!std::isnan(create_order.price)) {
    fmt::format_to(std::back_inserter(buffer), "&price={}"sv, Decimal{create_order.price, order.price_precision.precision});
  }
  fmt::format_to(std::back_inserter(buffer), "&quantity={}"sv, Decimal{create_order.quantity, order.quantity_precision.precision});
  fmt::format_to(std::back_inserter(buffer), "&recvWindow={}"sv, recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{}) {
    fmt::format_to(std::back_inserter(buffer), "&selfTradePreventionMode={}"sv, create_order_template.self_trade_prevention_mode.as_raw_text());
  }
  fmt::format_to(std::back_inserter(buffer), "&side={}"sv, side.as_raw_text());
  if (side_effect_type != SideEffectType::UNDEFINED) {
    fmt::format_to(std::back_inserter(buffer), "&sideEffectType={}"sv, magic_enum::enum_name(side_effect_type));
  }
  if (!std::isnan(create_order.stop_price)) {
    fmt::format_to(std::back_inserter(buffer), "&stopPrice={}"sv, Decimal{create_order.stop_price, order.price_precision.precision});
  }
  fmt::format_to(std::back_inserter(buffer), "&symbol={}"sv, create_order.symbol);
  if (time_in_force != json::TimeInForce{}) {
    fmt::format_to(std::back_inserter(buffer), "&timeInForce={}"sv, time_in_force.as_raw_text());
  }
  fmt::format_to(std::back_inserter(buffer), "&type={}"sv, type.as_raw_text());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
#else
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  if (create_order.margin_mode == MarginMode::ISOLATED) {
    writer.write("isIsolated=TRUE&"sv);
  }
  writer.write("newClientOrderId="sv).write(request_id);
  if (!std::isnan(create_order.price)) {
    writer.write("&price="sv).write(Decimal{create_order.price, order.price_precision.precision});
  }
  writer.write("&quantity="sv).write(Decimal{create_order.quantity, order.quantity_precision.precision});
  writer.write("&recvWindow="sv).write(recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{}) {
    writer.write("&selfTradePreventionMode="sv).write(create_order_template.self_trade_prevention_mode.as_raw_text());
  }
  writer.write("&side="sv).write(side.as_raw_text());
  if (side_effect_type != SideEffectType::UNDEFINED) {
    writer.write("&sideEffectType="sv).write(magic_enum::enum_name(side_effect_type));
  }
  if (!std::isnan(create_order.stop_price)) {
    writer.write("&stopPrice="sv).write(Decimal{create_order.stop_price, order.price_precision.precision});
  }
  writer.write("&symbol="sv).write(create_order.symbol);
  if (time_in_force != json::TimeInForce{}) {
    writer.write("&timeInForce="sv).write(time_in_force.as_raw_text());
  }
  writer.write("&type="sv).write(type.as_raw_text());
  return writer.finish();
#endif
}

std::string_view Encoder::cancel_order_url(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    server::oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    CancelOrderTemplate const &cancel_order_template,
    std::chrono::milliseconds recv_window) {
#if defined(WS_USE_FMT)
  buffer.clear();
  if (order.margin_mode == MarginMode::ISOLATED) {
    fmt::format_to(std::back_inserter(buffer), "isIsolated=TRUE&"sv);
  }
  fmt::format_to(std::back_inserter(buffer), "newClientOrderId={}"sv, request_id);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{}) {
    fmt::format_to(std::back_inserter(buffer), "&cancelRestrictions={}"sv, cancel_order_template.cancel_restrictions.as_raw_text());
  }
  if (!std::empty(order.external_order_id)) {
    fmt::format_to(std::back_inserter(buffer), "&orderId={}"sv, order.external_order_id);
  }
  fmt::format_to(
      std::back_inserter(buffer),
      "&origClientOrderId={}"
      "&recvWindow={}"
      "&symbol={}"sv,
      previous_request_id,
      recv_window.count(),
      order.symbol);
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
#else
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  if (order.margin_mode == MarginMode::ISOLATED) {
    writer.write("isIsolated=TRUE&"sv);
  }
  writer.write("newClientOrderId="sv).write(request_id);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{}) {
    writer.write("&cancelRestrictions="sv).write(cancel_order_template.cancel_restrictions.as_raw_text());
  }
  if (!std::empty(order.external_order_id)) {
    writer.write("&orderId="sv).write(order.external_order_id);
  }
  writer.write("&origClientOrderId="sv).write(previous_request_id);
  writer.write("&recvWindow="sv).write(recv_window.count());
  writer.write("&symbol="sv).write(order.symbol);
  return writer.finish();
#endif
}

// cancel-all

std::string_view Encoder::cancel_all_open_orders_url(
    std::vector<char> &buffer, std::string_view const &symbol, MarginMode margin_mode, std::chrono::milliseconds recv_window) {
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
      R"(recvWindow={})"sv,
      symbol,
      recv_window.count());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
