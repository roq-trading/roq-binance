/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/json/wsapi_parser_2.hpp"

#include <utility>

#include "roq/logging.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/wsapi_type.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

namespace {
auto get_request(auto &message) -> WSAPIRequest {
  core::json::Parser parser{message};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key.compare("id"sv) == 0) {
      auto id = core::json::get<std::string_view>(value);
      return WSAPIRequest::decode(id);
    }
  }
  return {};
}

bool dispatch_listen_key(auto &handler, auto &message, auto &buffer, auto &trace_info, auto &request) {
  WSAPIListenKey listen_key{message, buffer};
  create_trace_and_dispatch(handler, trace_info, listen_key, request);
  return true;
}

bool dispatch_account_status(auto &handler, auto &message, auto &buffer, auto &trace_info, auto &request) {
  WSAPIAccount account{message, buffer};
  create_trace_and_dispatch(handler, trace_info, account, request);
  return true;
}

bool dispatch_open_orders_status(auto &handler, auto &message, auto &buffer, auto &trace_info, auto &request) {
  WSAPIOpenOrders open_orders{message, buffer};
  create_trace_and_dispatch(handler, trace_info, open_orders, request);
  return true;
}

bool dispatch_my_trades(auto &handler, auto &message, auto &buffer, auto &trace_info, auto &request) {
  WSAPITrades trades{message, buffer};
  create_trace_and_dispatch(handler, trace_info, trades, request);
  return true;
}

bool dispatch_open_orders_cancel_all(auto &handler, auto &message, auto &buffer, auto &trace_info, auto &request) {
  WSAPICancelOpenOrders cancel_open_orders{message, buffer};
  create_trace_and_dispatch(handler, trace_info, cancel_open_orders, request);
  return true;
}

bool dispatch_order_place(auto &handler, auto &message, auto &buffer, auto &trace_info, auto &request) {
  WSAPIOrderPlace order_place{message, buffer};
  create_trace_and_dispatch(handler, trace_info, order_place, request);
  return true;
}

bool dispatch_order_cancel(auto &handler, auto &message, auto &buffer, auto &trace_info, auto &request) {
  WSAPICancelOrder cancel_order{message, buffer};
  create_trace_and_dispatch(handler, trace_info, cancel_order, request);
  return true;
}

bool dispatch_order_cancel_replace(auto &handler, auto &message, auto &buffer, auto &trace_info, auto &request) {
  WSAPICancelReplaceOrder cancel_replace_order{message, buffer};
  create_trace_and_dispatch(handler, trace_info, cancel_replace_order, request);
  return true;
}
}  // namespace

bool WSAPIParser2::dispatch(
    WSAPIParser2::Handler &handler,
    std::string_view const &message,
    std::span<std::byte> const &buffer,
    TraceInfo const &trace_info) {
  auto request = get_request(message);
  switch (request.type) {
    using enum WSAPIType::type_t;
    case UNDEFINED__:
    case UNKNOWN__:
      break;
    case LISTEN_KEY_CREATE:
      return dispatch_listen_key(handler, message, buffer, trace_info, request);
    case LISTEN_KEY_PING:
      return true;  // note!
    case ACCOUNT_STATUS:
      return dispatch_account_status(handler, message, buffer, trace_info, request);
    case OPEN_ORDERS_STATUS:
      return dispatch_open_orders_status(handler, message, buffer, trace_info, request);
    case MY_TRADES:
      return dispatch_my_trades(handler, message, buffer, trace_info, request);
    case OPEN_ORDERS_CANCEL_ALL:
      return dispatch_open_orders_cancel_all(handler, message, buffer, trace_info, request);
    case ORDER_PLACE:
      return dispatch_order_place(handler, message, buffer, trace_info, request);
    case ORDER_CANCEL:
      return dispatch_order_cancel(handler, message, buffer, trace_info, request);
    case ORDER_CANCEL_REPLACE:
      return dispatch_order_cancel_replace(handler, message, buffer, trace_info, request);
  }
  return false;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
