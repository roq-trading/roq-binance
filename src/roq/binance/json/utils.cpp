/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/json/utils.hpp"

#include "roq/logging.hpp"

#include "roq/utils/number.hpp"

#include "roq/utils/text/writer.hpp"

using namespace std::literals;

using namespace fmt::literals;

namespace roq {
namespace binance {
namespace json {

Error guess_error(int32_t code) {
  switch (code) {
      // https://binance-docs.github.io/apidocs/spot/en/#10xx-general-server-or-network-issues
    case -1000:  // UNKNOWN
      break;
    case -1001:  // DISCONNECTED
      break;
    case -1002:  // UNAUTHORIZED
      return Error::NOT_AUTHORIZED;
    case -1003:  // TOO_MANY_REQUESTS
      return Error::REQUEST_RATE_LIMIT_REACHED;
    case -1004:  // SERVER_BUSY
      break;
    case -1006:  // UNEXPECTED_RESP
      break;
    case -1007:  // TIMEOUT
      break;
    case -1014:  // UNKNOWN_ORDER_COMPOSITION
      break;
    case -1015:  // TOO_MANY_ORDERS
      return Error::REQUEST_RATE_LIMIT_REACHED;
    case -1016:  // SERVICE_SHUTTING_DOWN
      break;
    case -1020:  // UNSUPPORTED_OPERATION
      break;
    case -1021:  // INVALID_TIMESTAMP
      return Error::INVALID_REQUEST_ARGS;
    case -1022:  // INVALID_SIGNATURE
      return Error::NOT_AUTHORIZED;
    case -1099:  //  Not found, authenticated, or authorized
      return Error::NOT_AUTHORIZED;
    // https://binance-docs.github.io/apidocs/spot/en/#11xx-2xxx-request-issues
    case -1100:  // ILLEGAL_CHARS
      return Error::INVALID_REQUEST_ARGS;
    case -1101:  // TOO_MANY_PARAMETERS
      return Error::INVALID_REQUEST_ARGS;
    case -1102:  // MANDATORY_PARAM_EMPTY_OR_MALFORMED
      return Error::INVALID_REQUEST_ARGS;
    case -1103:  // UNKNOWN_PARAM
      return Error::INVALID_REQUEST_ARGS;
    case -1104:  // UNREAD_PARAMETERS
      return Error::INVALID_REQUEST_ARGS;
    case -1105:  // PARAM_EMPTY
      return Error::INVALID_REQUEST_ARGS;
    case -1106:  // PARAM_NOT_REQUIRED
      return Error::INVALID_REQUEST_ARGS;
    case -1111:  // BAD_PRECISION
      return Error::INVALID_REQUEST_ARGS;
    case -1112:  // NO_DEPTH
      break;
    case -1114:  // TIF_NOT_REQUIRED
      return Error::INVALID_TIME_IN_FORCE;
    case -1115:  // INVALID_TIF
      return Error::INVALID_TIME_IN_FORCE;
    case -1116:  // INVALID_ORDER_TYPE
      return Error::INVALID_ORDER_TYPE;
    case -1117:  // INVALID_SIDE
      return Error::INVALID_SIDE;
    case -1118:  // EMPTY_NEW_CL_ORD_ID
      return Error::INVALID_REQUEST_ID;
    case -1119:  // EMPTY_ORG_CL_ORD_ID
      return Error::INVALID_REQUEST_ID;
    case -1120:  // BAD_INTERVAL
      break;
    case -1121:  // BAD_SYMBOL
      return Error::INVALID_SYMBOL;
    case -1125:  // INVALID_LISTEN_KEY
      return Error::NOT_AUTHORIZED;
    case -1127:  // MORE_THAN_XX_HOURS
      break;
    case -1128:  // OPTIONAL_PARAMS_BAD_COMBO
      return Error::INVALID_REQUEST_ARGS;
    case -1130:  // INVALID_PARAMETER
      return Error::INVALID_REQUEST_ARGS;
    case -1131:  // BAD_RECV_WINDOW
      return Error::INVALID_REQUEST_ARGS;
    case -2008:
      return Error::NOT_AUTHORIZED;
    case -2010:  // NEW_ORDER_REJECTED
      break;
    case -2011:  // CANCEL_REJECTED
      return Error::TOO_LATE_TO_MODIFY_OR_CANCEL;
    case -2013:  // NO_SUCH_ORDER
      return Error::UNKNOWN_ORDER_ID;
    case -2014:  // BAD_API_KEY_FMT
      break;
    case -2015:  // REJECTED_MBX_KEY
      break;
    case -2016:  // NO_TRADING_WINDOW
      break;
      // https://binance-docs.github.io/apidocs/spot/en/#3xxx-5xxx-sapi-specific-issues
      // https://binance-docs.github.io/apidocs/spot/en/#6xxx-savings-issues
      // ...
  }
  return Error::UNKNOWN;
}

namespace {
json::OrderType map_order_type(auto &order) {
  switch (order.order_type) {
    using enum roq::OrderType;
    case UNDEFINED:
      break;
    case MARKET:
      if (!std::isnan(order.stop_price))
        return json::OrderType::STOP_LOSS;
      return json::OrderType::MARKET;
    case LIMIT:
      if (order.execution_instructions.has(ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE))
        return json::OrderType::LIMIT_MAKER;
      if (!std::isnan(order.stop_price))
        return json::OrderType::STOP_LOSS_LIMIT;
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
      if (create_order.execution_instructions.has(ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE))
        return {};
      break;
  }
  return map(create_order.time_in_force);
}
}  // namespace

// my-trades

std::string_view my_trades(
    std::vector<char> &buffer,
    std::string_view const &symbol,
    std::chrono::nanoseconds lookback,
    uint32_t limit,
    std::chrono::milliseconds now) {
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - lookback);
  writer.write("limit="sv).write(limit);
  writer.write("&startTime="sv).write(start_time.count());
  writer.write("&symbol="sv).write(symbol);
  return writer.finish();
}

// cancel-all

std::string_view cancel_all_open_orders(
    std::vector<char> &buffer, std::string_view const &symbol, std::chrono::milliseconds recv_window) {
  buffer.clear();
  fmt::format_to(
      std::back_inserter(buffer),
      R"(symbol={}&)"
      R"(recvWindow={})"_cf,
      symbol,
      recv_window.count());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
}

// new

// https://binance-docs.github.io/apidocs/spot/en/#new-order-trade
std::string_view new_order(
    std::vector<char> &buffer,
    CreateOrder const &create_order,
    oms::Order const &order,
    std::string_view const &request_id,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window) {
  auto side = map(create_order.side);
  auto type = map_order_type(create_order);
  auto time_in_force = map_time_in_force(create_order);
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  writer.write("newClientOrderId="sv).write(request_id);
  if (!std::isnan(create_order.price))
    writer.write("&price="sv).write(utils::Number{create_order.price, order.price_precision.decimals});
  writer.write("&quantity="sv).write(utils::Number{create_order.quantity, order.quantity_precision.decimals});
  writer.write("&recvWindow="sv).write(recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{})
    writer.write("&selfTradePreventionMode="sv).write(create_order_template.self_trade_prevention_mode.as_raw_text());
  writer.write("&side="sv).write(side.as_raw_text());
  if (!std::isnan(create_order.stop_price))
    writer.write("&stopPrice="sv).write(utils::Number{create_order.stop_price, order.price_precision.decimals});
  writer.write("&symbol="sv).write(create_order.symbol);
  if (time_in_force != json::TimeInForce{})
    writer.write("&timeInForce="sv).write(time_in_force.as_raw_text());
  writer.write("&type="sv).write(type.as_raw_text());
  return writer.finish();
}

std::string_view new_order_ws_url(
    std::vector<char> &buffer,
    CreateOrder const &create_order,
    oms::Order const &order,
    std::string_view const &request_id,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now) {
  auto side = map(create_order.side);
  auto type = map_order_type(create_order);
  auto time_in_force = map_time_in_force(create_order);
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  if (!std::empty(api_key))
    writer.write("apiKey="sv).write(api_key).write("&"sv);
  writer.write("newClientOrderId="sv).write(request_id);
  if (!std::isnan(create_order.price))
    writer.write("&price="sv).write(utils::Number{create_order.price, order.price_precision.decimals});
  writer.write("&quantity="sv).write(utils::Number{create_order.quantity, order.quantity_precision.decimals});
  writer.write("&recvWindow="sv).write(recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{})
    writer.write("&selfTradePreventionMode="sv).write(create_order_template.self_trade_prevention_mode.as_raw_text());
  writer.write("&side="sv).write(side.as_raw_text());
  if (!std::isnan(create_order.stop_price))
    writer.write("&stopPrice="sv).write(utils::Number{create_order.stop_price, order.price_precision.decimals});
  writer.write("&symbol="sv).write(create_order.symbol);
  if (time_in_force != json::TimeInForce{})
    writer.write("&timeInForce="sv).write(time_in_force.as_raw_text());
  writer.write("&timestamp="sv).write(now.count());
  writer.write("&type="sv).write(type.as_raw_text());
  return writer.finish();
}

std::string_view new_order_ws_json(
    std::vector<char> &buffer,
    CreateOrder const &create_order,
    oms::Order const &order,
    std::string_view const &request_id,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now,
    std::string_view const &signature) {
  auto side = map(create_order.side);
  auto type = map_order_type(create_order);
  auto time_in_force = map_time_in_force(create_order);
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  writer.write("{"sv);
  writer.write(R"("apiKey":")"sv).write(api_key).write(R"(")"sv);
  writer.write(R"(,"newClientOrderId":")"sv).write(request_id).write(R"(")"sv);
  if (!std::isnan(create_order.price))
    writer.write(R"(,"price":")"sv)
        .write(utils::Number{create_order.price, order.price_precision.decimals})
        .write(R"(")"sv);
  writer.write(R"(,"quantity":")"sv)
      .write(utils::Number{create_order.quantity, order.quantity_precision.decimals})
      .write(R"(")"sv);
  writer.write(R"(,"recvWindow":)"sv).write(recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{})
    writer.write(R"(,"selfTradePreventionMode":")"sv)
        .write(create_order_template.self_trade_prevention_mode.as_raw_text())
        .write(R"(")"sv);
  writer.write(R"(,"side":")"sv).write(side.as_raw_text()).write(R"(")"sv);
  if (!std::isnan(create_order.stop_price))
    writer.write(R"(,"stopPrice":")"sv)
        .write(utils::Number{create_order.stop_price, order.price_precision.decimals})
        .write(R"(")"sv);
  writer.write(R"(,"symbol":")"sv).write(create_order.symbol).write(R"(")"sv);
  if (time_in_force != json::TimeInForce{})
    writer.write(R"(,"timeInForce":")"sv).write(time_in_force.as_raw_text()).write(R"(")"sv);
  writer.write(R"(,"timestamp":)"sv).write(now.count());
  writer.write(R"(,"type":")"sv).write(type.as_raw_text()).write(R"(")"sv);
  writer.write(R"(,"signature":")"sv).write(signature).write(R"(")"sv);
  writer.write("}"sv);
  return writer.finish();
}

// cancel

std::string_view cancel_order(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    CancelOrderTemplate const &cancel_order_template,
    std::chrono::milliseconds recv_window) {
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  writer.write("newClientOrderId="sv).write(request_id);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{})
    writer.write("&cancelRestrictions="sv).write(cancel_order_template.cancel_restrictions.as_raw_text());
  if (!std::empty(order.external_order_id))
    writer.write("&orderId="sv).write(order.external_order_id);
  writer.write("&origClientOrderId="sv).write(previous_request_id);
  writer.write("&recvWindow="sv).write(recv_window.count());
  writer.write("&symbol="sv).write(order.symbol);
  return writer.finish();
}

std::string_view cancel_order_ws_url(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    CancelOrderTemplate const &cancel_order_template,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now) {
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  writer.write("apiKey="sv).write(api_key);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{})
    writer.write("&cancelRestrictions="sv).write(cancel_order_template.cancel_restrictions.as_raw_text());
  writer.write("&newClientOrderId="sv).write(request_id);
  if (!std::empty(order.external_order_id))
    writer.write("&orderId="sv).write(order.external_order_id);
  writer.write("&origClientOrderId="sv).write(previous_request_id);
  writer.write("&recvWindow="sv).write(recv_window.count());
  writer.write("&symbol="sv).write(order.symbol);
  writer.write("&timestamp="sv).write(now.count());
  return writer.finish();
}

std::string_view cancel_order_ws_json(
    std::vector<char> &buffer,
    roq::CancelOrder const &,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id,
    CancelOrderTemplate const &cancel_order_template,
    std::chrono::milliseconds recv_window,
    std::string_view const &api_key,
    std::chrono::milliseconds now,
    std::string_view const &signature) {
  buffer.resize(512);
  std::span buffer_2{reinterpret_cast<std::byte *>(std::data(buffer)), std::size(buffer)};
  utils::text::Writer writer{buffer_2};
  writer.write("{"sv);
  writer.write(R"("apiKey":")"sv).write(api_key).write(R"(")"sv);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{})
    writer.write(R"(,"cancelRestrictions":")"sv)
        .write(cancel_order_template.cancel_restrictions.as_raw_text())
        .write(R"(")"sv);
  writer.write(R"(,"newClientOrderId":")"sv).write(request_id).write(R"(")"sv);
  if (!std::empty(order.external_order_id))
    writer.write(R"(,"orderId":)"sv).write(order.external_order_id);  // note! integer
  writer.write(R"(,"origClientOrderId":")"sv).write(previous_request_id).write(R"(")"sv);
  writer.write(R"(,"recvWindow":)"sv).write(recv_window.count());
  writer.write(R"(,"symbol":")"sv).write(order.symbol).write(R"(")"sv);
  writer.write(R"(,"timestamp":)"sv).write(now.count());
  writer.write(R"(,"signature":")"sv).write(signature).write(R"(")"sv);
  writer.write("}"sv);
  return writer.finish();
}

// cancel-replace

namespace {
auto get_cancel_replace_mode(auto &cancel_order_template, bool stop_on_failure) {
  switch (cancel_order_template.cancel_replace_mode) {
    using enum CancelReplaceMode::type_t;
    case UNDEFINED__:
      break;
    case UNKNOWN__:
      log::fatal("Unexpected"sv);
      break;
    case STOP_ON_FAILURE:
    case ALLOW_FAILURE:
      return cancel_order_template.cancel_replace_mode.as_raw_text();
  }
  return stop_on_failure ? "STOP_ON_FAILURE"sv : "ALLOW_FAILURE"sv;
}
}  // namespace

// https://binance-docs.github.io/apidocs/spot/en/#cancel-an-existing-order-and-send-a-new-order-trade
std::string_view cancel_replace_order(
    std::vector<char> &buffer,
    std::string_view const &cancel_request_id,
    std::string_view const &cancel_previous_request_id,
    std::string_view const &cancel_external_order_id,
    CreateOrder const &create_order,
    oms::Order const &order,
    std::string_view const &create_request_id,
    CancelOrderTemplate const &cancel_order_template,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window,
    bool stop_on_failure) {
  auto side = map(order.side);
  auto type = map_order_type(order);
  auto time_in_force = map_time_in_force(order);
  auto cancel_replace_mode = get_cancel_replace_mode(cancel_order_template, stop_on_failure);
  buffer.clear();
  fmt::format_to(std::back_inserter(buffer), "cancelNewClientOrderId={}&"sv, cancel_request_id);
  if (!std::empty(cancel_external_order_id))
    fmt::format_to(std::back_inserter(buffer), "cancelOrderId={}&"sv, cancel_external_order_id);
  fmt::format_to(std::back_inserter(buffer), "cancelOrigClientOrderId={}&"sv, cancel_previous_request_id);
  fmt::format_to(std::back_inserter(buffer), "cancelReplaceMode={}&"sv, cancel_replace_mode);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{})
    fmt::format_to(
        std::back_inserter(buffer),
        "cancelRestrictions={}&"sv,
        cancel_order_template.cancel_restrictions.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), "newClientOrderId={}&"sv, create_request_id);
  if (!std::isnan(create_order.price))
    fmt::format_to(
        std::back_inserter(buffer), "price={}&"sv, utils::Number{create_order.price, order.price_precision.decimals});
  fmt::format_to(
      std::back_inserter(buffer),
      "quantity={}&"sv,
      utils::Number{create_order.quantity, order.quantity_precision.decimals});
  fmt::format_to(std::back_inserter(buffer), "recvWindow={}&"sv, recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{})
    fmt::format_to(
        std::back_inserter(buffer),
        "selfTradePreventionMode={}&"sv,
        create_order_template.self_trade_prevention_mode.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), "side={}&"sv, side.as_raw_text());
  if (!std::isnan(create_order.stop_price))
    fmt::format_to(
        std::back_inserter(buffer),
        "stopPrice={}&"sv,
        utils::Number{create_order.stop_price, order.price_precision.decimals});
  fmt::format_to(std::back_inserter(buffer), "symbol={}&"sv, order.symbol);
  if (time_in_force != json::TimeInForce{})
    fmt::format_to(std::back_inserter(buffer), "timeInForce={}&"sv, time_in_force.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), "type={}"sv, type.as_raw_text());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
}

std::string_view cancel_replace_order_ws_url(
    std::vector<char> &buffer,
    std::string_view const &cancel_request_id,
    std::string_view const &cancel_previous_request_id,
    std::string_view const &cancel_external_order_id,
    CreateOrder const &create_order,
    oms::Order const &order,
    std::string_view const &create_request_id,
    CancelOrderTemplate const &cancel_order_template,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window,
    bool stop_on_failure,
    std::string_view const &api_key,
    std::chrono::milliseconds now) {
  auto side = map(order.side);
  auto type = map_order_type(order);
  auto time_in_force = map_time_in_force(order);
  auto cancel_replace_mode = get_cancel_replace_mode(cancel_order_template, stop_on_failure);
  buffer.clear();
  fmt::format_to(std::back_inserter(buffer), "apiKey={}&"sv, api_key);
  fmt::format_to(std::back_inserter(buffer), "cancelNewClientOrderId={}&"sv, cancel_request_id);
  if (!std::empty(cancel_external_order_id))
    fmt::format_to(std::back_inserter(buffer), "cancelOrderId={}&"sv, cancel_external_order_id);
  fmt::format_to(std::back_inserter(buffer), "cancelOrigClientOrderId={}&"sv, cancel_previous_request_id);
  fmt::format_to(std::back_inserter(buffer), "cancelReplaceMode={}&"sv, cancel_replace_mode);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{})
    fmt::format_to(
        std::back_inserter(buffer),
        "cancelRestrictions={}&"sv,
        cancel_order_template.cancel_restrictions.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), "newClientOrderId={}&"sv, create_request_id);
  if (!std::isnan(create_order.price))
    fmt::format_to(
        std::back_inserter(buffer), "price={}&"sv, utils::Number{create_order.price, order.price_precision.decimals});
  fmt::format_to(
      std::back_inserter(buffer),
      "quantity={}&"sv,
      utils::Number{create_order.quantity, order.quantity_precision.decimals});
  fmt::format_to(std::back_inserter(buffer), "recvWindow={}&"sv, recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{})
    fmt::format_to(
        std::back_inserter(buffer),
        "selfTradePreventionMode={}&"sv,
        create_order_template.self_trade_prevention_mode.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), "side={}&"sv, side.as_raw_text());
  if (!std::isnan(create_order.stop_price))
    fmt::format_to(
        std::back_inserter(buffer),
        "stopPrice={}&"sv,
        utils::Number{create_order.stop_price, order.price_precision.decimals});
  fmt::format_to(std::back_inserter(buffer), "symbol={}&"sv, order.symbol);
  if (time_in_force != json::TimeInForce{})
    fmt::format_to(std::back_inserter(buffer), "timeInForce={}&"sv, time_in_force.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), "timestamp={}&"sv, now.count());
  fmt::format_to(std::back_inserter(buffer), "type={}"sv, type.as_raw_text());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
}

std::string_view cancel_replace_order_ws_json(
    std::vector<char> &buffer,
    std::string_view const &cancel_request_id,
    std::string_view const &cancel_previous_request_id,
    std::string_view const &cancel_external_order_id,
    CreateOrder const &create_order,
    oms::Order const &order,
    std::string_view const &create_request_id,
    CancelOrderTemplate const &cancel_order_template,
    CreateOrderTemplate const &create_order_template,
    std::chrono::milliseconds recv_window,
    bool stop_on_failure,
    std::string_view const &api_key,
    std::chrono::milliseconds now,
    std::string_view const &signature) {
  auto side = map(order.side);
  auto type = map_order_type(order);
  auto time_in_force = map_time_in_force(order);
  auto cancel_replace_mode = get_cancel_replace_mode(cancel_order_template, stop_on_failure);
  buffer.clear();
  fmt::format_to(std::back_inserter(buffer), "{{"sv);
  fmt::format_to(std::back_inserter(buffer), R"("apiKey":"{}",)"sv, api_key);
  fmt::format_to(std::back_inserter(buffer), R"("cancelNewClientOrderId":"{}",)"sv, cancel_request_id);
  if (!std::empty(cancel_external_order_id))
    fmt::format_to(std::back_inserter(buffer), R"("cancelOrderId":{},)"sv, cancel_external_order_id);
  fmt::format_to(std::back_inserter(buffer), R"("cancelOrigClientOrderId":"{}",)"sv, cancel_previous_request_id);
  fmt::format_to(std::back_inserter(buffer), R"("cancelReplaceMode":"{}",)"sv, cancel_replace_mode);
  if (cancel_order_template.cancel_restrictions != CancelRestrictions{})
    fmt::format_to(
        std::back_inserter(buffer),
        R"("cancelRestrictions":"{}",)"sv,
        cancel_order_template.cancel_restrictions.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), R"("newClientOrderId":"{}",)"sv, create_request_id);
  if (!std::isnan(create_order.price))
    fmt::format_to(
        std::back_inserter(buffer),
        R"("price":"{}",)"sv,
        utils::Number{create_order.price, order.price_precision.decimals});
  fmt::format_to(
      std::back_inserter(buffer),
      R"("quantity":"{}",)"sv,
      utils::Number{create_order.quantity, order.quantity_precision.decimals});
  fmt::format_to(std::back_inserter(buffer), R"("recvWindow":{},)"sv, recv_window.count());
  if (create_order_template.self_trade_prevention_mode != SelfTradePreventionMode{})
    fmt::format_to(
        std::back_inserter(buffer),
        R"("selfTradePreventionMode":"{}",)"sv,
        create_order_template.self_trade_prevention_mode.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), R"("side":"{}",)"sv, side.as_raw_text());
  if (!std::isnan(create_order.stop_price))
    fmt::format_to(
        std::back_inserter(buffer),
        R"("stopPrice":"{}",)"sv,
        utils::Number{create_order.stop_price, order.price_precision.decimals});
  fmt::format_to(std::back_inserter(buffer), R"("symbol":"{}",)"sv, order.symbol);
  if (time_in_force != json::TimeInForce{})
    fmt::format_to(std::back_inserter(buffer), R"("timeInForce":"{}",)"sv, time_in_force.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), R"("timestamp":{},)"sv, now.count());
  fmt::format_to(std::back_inserter(buffer), R"("type":"{}",)"sv, type.as_raw_text());
  fmt::format_to(std::back_inserter(buffer), R"("signature":"{}")"sv, signature);
  fmt::format_to(std::back_inserter(buffer), "}}"sv);
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
