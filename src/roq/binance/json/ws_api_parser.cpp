/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/json/ws_api_parser.hpp"

#include <utility>

#include "roq/logging.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/ws_api_type.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

namespace {
bool dispatch_error(auto &handler, auto status, auto &value, auto &buffer, auto &trace_info, auto &request) {
  Error error{value, buffer};
  create_trace_and_dispatch(handler, trace_info, error, request);
  return true;
}

bool dispatch_listen_key(auto &handler, auto &value, auto &buffer, auto &trace_info, auto &request) {
  ListenKey listen_key{value, buffer};
  create_trace_and_dispatch(handler, trace_info, listen_key, request);
  return true;
}

bool dispatch_account_status(auto &handler, auto &value, auto &buffer, auto &trace_info, auto &request) {
  Account account{value, buffer};
  create_trace_and_dispatch(handler, trace_info, account, request);
  return true;
}

bool dispatch_open_orders_status(auto &handler, auto &value, auto &buffer, auto &trace_info, auto &request) {
  json::OpenOrders open_orders{value, buffer};
  create_trace_and_dispatch(handler, trace_info, open_orders, request);
  return true;
}

bool dispatch_open_orders_cancel_all(auto &handler, auto &value, auto &buffer, auto &trace_info, auto &request) {
  json::CancelAllOpenOrders cancel_all_open_orders{value, buffer};
  create_trace_and_dispatch(handler, trace_info, cancel_all_open_orders, request);
  return true;
}

bool dispatch_order_place(auto &handler, auto &value, auto &buffer, auto &trace_info, auto &request) {
  json::NewOrder new_order{value, buffer};
  create_trace_and_dispatch(handler, trace_info, new_order, request);
  return true;
}

bool dispatch_order_cancel(auto &handler, auto &value, auto &buffer, auto &trace_info, auto &request) {
  json::CancelOrder cancel_order{value, buffer};
  create_trace_and_dispatch(handler, trace_info, cancel_order, request);
  return true;
}

bool dispatch_helper(auto &handler, auto status, auto &value, auto &buffer, auto &trace_info, auto &request) {
  switch (request.type) {
    using enum WSAPIType::type_t;
    case UNDEFINED:
    case UNKNOWN:
      break;
    case LISTEN_KEY_CREATE:
      return dispatch_listen_key(handler, value, buffer, trace_info, request);
    case LISTEN_KEY_PING:
      return true;  // note!
    case ACCOUNT_STATUS:
      return dispatch_account_status(handler, value, buffer, trace_info, request);
    case OPEN_ORDERS_STATUS:
      return dispatch_open_orders_status(handler, value, buffer, trace_info, request);
    case OPEN_ORDERS_CANCEL_ALL:
      return dispatch_open_orders_cancel_all(handler, value, buffer, trace_info, request);
    case ORDER_PLACE:
      return dispatch_order_place(handler, value, buffer, trace_info, request);
    case ORDER_CANCEL:
      return dispatch_order_cancel(handler, value, buffer, trace_info, request);
  }
  return false;
}
}  // namespace

bool WSAPIParser::dispatch(
    WSAPIParser::Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    TraceInfo const &trace_info) {
  std::string_view id;
  WSAPIRequest request;
  int32_t status = {};
  for (size_t i = 0; i < 2; ++i) {
    core::json::Parser parser(message);
    auto root = parser.root();
    for (auto [key, value] : std::get<core::json::Object>(root)) {
      if (key.compare("id"sv) == 0) {
        id = core::json::get<std::string_view>(value);
        request = WSAPIRequest::decode(id);
      } else if (key.compare("status"sv) == 0) {
        status = core::json::get<int32_t>(value);
      } else if (key.compare("result"sv) == 0) {
        if (!std::empty(id) && status != 0)
          return dispatch_helper(handler, status, value, buffer, trace_info, request);
      } else if (key.compare("error"sv) == 0) {
        if (!std::empty(id) && status != 0)
          return dispatch_error(handler, status, value, buffer, trace_info, request);
      } else if (key.compare("rateLimits"sv) == 0) {
        // drop
      }
    }
  }
  return false;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
