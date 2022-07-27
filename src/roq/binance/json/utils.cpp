/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/json/utils.hpp"

#include "roq/logging.hpp"

#include "roq/utils/number.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

Error guess_error(int32_t code) {
  switch (code) {
    case -1000:
      break;
    case -1001:
      break;
    case -1002:
      return Error::NOT_AUTHORIZED;
    case -1003:
      return Error::REQUEST_RATE_LIMIT_REACHED;
    case -1013:
      return Error::INVALID_REQUEST_ARGS;
    case -1015:
      return Error::REQUEST_RATE_LIMIT_REACHED;
    case -1016:
      break;
    case -1020:
    case -1021:
      return Error::INVALID_REQUEST_ARGS;
    case -1022:
      return Error::NOT_AUTHORIZED;
    case -1100:
      return Error::INVALID_REQUEST_ARGS;
    case -1101:
      return Error::INVALID_REQUEST_ARGS;
    case -1102:
      return Error::INVALID_REQUEST_ARGS;
    case -1103:
      return Error::INVALID_REQUEST_ARGS;
    case -1104:
      return Error::INVALID_REQUEST_ARGS;
    case -1105:
      return Error::INVALID_REQUEST_ARGS;
    case -1106:
      return Error::INVALID_REQUEST_ARGS;
    case -1111:
      return Error::INVALID_REQUEST_ARGS;
    case -1112:
      return Error::INVALID_ORDER_ID;
    case -1114:
      return Error::INVALID_TIME_IN_FORCE;
    case -1115:
      return Error::INVALID_TIME_IN_FORCE;
    case -1116:
      return Error::INVALID_ORDER_TYPE;
    case -1117:
      return Error::INVALID_SIDE;
    case -1118:
      return Error::INVALID_REQUEST_ID;
    case -1119:
      return Error::INVALID_REQUEST_ID;
    case -1120:
      break;
    case -1121:
      return Error::INVALID_SYMBOL;
    case -1125:
      return Error::NOT_AUTHORIZED;
    case -1127:
      break;
    case -1128:
      return Error::INVALID_REQUEST_ARGS;
    case -1130:
      return Error::INVALID_REQUEST_ARGS;
    case -1131:
      return Error::INVALID_REQUEST_ARGS;
    case -2008:
      return Error::NOT_AUTHORIZED;
    case -2010:
      return Error::INSUFFICIENT_FUNDS;
      break;
  }
  return Error::UNKNOWN;
  /*
  '-2011': OrderNotFound,  # cancelOrder(1, 'BTC/USDT') -> 'UNKNOWN_ORDER'
  '-2013': OrderNotFound,  # fetchOrder(1, 'BTC/USDT') -> 'Order does not exist'
  '-2014': AuthenticationError,  # {"code":-2014, "msg": "API-key format invalid."}
  '-2015': AuthenticationError,  # "Invalid API-key, IP, or permissions for action."
  '-2019': InsufficientFunds,  # {"code":-2019,"msg":"Margin is insufficient."}
  '-3005': InsufficientFunds,  # {"code":-3005,"msg":"Transferring out not allowed. Transfer out
  amount exceeds max amount."}
  '-3006': InsufficientFunds,  # {"code":-3006,"msg":"Your borrow amount has exceed maximum borrow
  amount."}
  '-3008': InsufficientFunds,  # {"code":-3008,"msg":"Borrow not allowed. Your borrow amount has
  exceed maximum borrow amount."}
  '-3010': ExchangeError,  # {"code":-3010,"msg":"Repay not allowed. Repay amount exceeds borrow
  amount."}
  '-3015': ExchangeError,  # {"code":-3015,"msg":"Repay amount exceeds borrow amount."}
  '-3022': AccountSuspended,  # You account's trading is banned.
  '-4028': BadRequest,  # {"code":-4028,"msg":"Leverage 100 is not valid"}
  '-3020': InsufficientFunds,  # {"code":-3020,"msg":"Transfer out amount exceeds max amount."}
  '-3041': InsufficientFunds,  # {"code":-3041,"msg":"Balance is not enough"}
  '-5013': InsufficientFunds,  # Asset transfer failed: insufficient balance"
  '-11008': InsufficientFunds,  # {"code":-11008,"msg":"Exceeding the account's maximum borrowable
  limit."}
  '-4051': InsufficientFunds,  # {"code":-4051,"msg":"Isolated balance insufficient."}
  */
}

namespace {
json::OrderType map_order_type(auto &create_order) {
  switch (create_order.order_type) {
    using enum roq::OrderType;
    case UNDEFINED:
      break;
    case MARKET:
      if (!std::isnan(create_order.stop_price))
        return json::OrderType::STOP_LOSS;
      return json::OrderType::MARKET;
    case LIMIT:
      if (create_order.execution_instructions.has(ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE))
        return json::OrderType::LIMIT_MAKER;
      if (!std::isnan(create_order.stop_price))
        return json::OrderType::STOP_LOSS_LIMIT;
      return json::OrderType::LIMIT;
  }
  return json::map(create_order.order_type);
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
  return json::map(create_order.time_in_force);
}
}  // namespace

std::string_view new_order(
    std::vector<char> &buffer,
    CreateOrder const &create_order,
    oms::Order const &order,
    std::string_view const &request_id,
    std::chrono::milliseconds recv_window) {
  auto side = map(create_order.side);
  auto type = map_order_type(create_order);
  auto time_in_force = map_time_in_force(create_order);
  buffer.clear();
  fmt::format_to(
      std::back_inserter(buffer),
      R"(symbol={}&)"
      R"(side={}&)"
      R"(type={}&)"
      R"(quantity={}&)"sv,
      create_order.symbol,
      side.as_raw_text(),
      type.as_raw_text(),
      utils::Number{create_order.quantity, order.quantity_decimals});
  if (time_in_force != json::TimeInForce{})
    fmt::format_to(std::back_inserter(buffer), R"(timeInForce={}&)"sv, time_in_force.as_raw_text());
  if (!std::isnan(create_order.price))
    fmt::format_to(
        std::back_inserter(buffer), R"(price={}&)"sv, utils::Number{create_order.price, order.price_decimals});
  if (!std::isnan(create_order.stop_price))
    fmt::format_to(
        std::back_inserter(buffer), R"(stopPrice={}&)"sv, utils::Number{create_order.stop_price, order.price_decimals});
  fmt::format_to(
      std::back_inserter(buffer),
      R"(newClientOrderId={}&)"
      R"(recvWindow={})"sv,
      request_id,
      recv_window.count());
  std::string_view result{std::data(buffer), std::size(buffer)};
  return result;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
