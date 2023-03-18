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
std::pair<std::string_view, WSAPIType> split(auto &id) {
  auto pos = id.find_first_of('-');
  if (pos != id.npos) {
    auto request_id = id.substr(0, pos);
    WSAPIType type{id.substr(pos + 1)};
    return {request_id, type};
  }
  return {};
}

bool dispatch_error(auto &handler, auto &id, auto status, auto &value, auto &buffer, auto &trace_info) {
  Error error{value, buffer};
  create_trace_and_dispatch(handler, trace_info, error);
  return true;
}

bool dispatch_listen_key(auto &handler, auto &value, auto &buffer, auto &trace_info) {
  ListenKey listen_key{value, buffer};
  create_trace_and_dispatch(handler, trace_info, listen_key);
  return true;
}

bool dispatch_account_status(auto &handler, auto &value, auto &buffer, auto &trace_info) {
  Account account{value, buffer};
  create_trace_and_dispatch(handler, trace_info, account);
  return true;
}

bool dispatch_open_orders_status(auto &handler, auto &value, auto &buffer, auto &trace_info) {
  json::OpenOrders open_orders{value, buffer};
  create_trace_and_dispatch(handler, trace_info, open_orders);
  return true;
}

bool dispatch_helper(auto &handler, auto &id, auto status, auto &value, auto &buffer, auto &trace_info) {
  auto [request_id, type] = split(id);
  switch (type) {
    using enum WSAPIType::type_t;
    case UNDEFINED:
    case UNKNOWN:
      break;
    case LISTEN_KEY:
      return dispatch_listen_key(handler, value, buffer, trace_info);
    case ACCOUNT_STATUS:
      return dispatch_account_status(handler, value, buffer, trace_info);
    case OPEN_ORDERS_STATUS:
      return dispatch_open_orders_status(handler, value, buffer, trace_info);
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
  int32_t status = {};
  for (size_t i = 0; i < 2; ++i) {
    core::json::Parser parser(message);
    auto root = parser.root();
    for (auto [key, value] : std::get<core::json::Object>(root)) {
      if (key.compare("id"sv) == 0) {
        id = core::json::get<std::string_view>(value);
      } else if (key.compare("status"sv) == 0) {
        status = core::json::get<int32_t>(value);
      } else if (key.compare("result"sv) == 0) {
        if (!std::empty(id) && status != 0)
          return dispatch_helper(handler, id, status, value, buffer, trace_info);
      } else if (key.compare("error"sv) == 0) {
        if (!std::empty(id) && status != 0)
          return dispatch_error(handler, id, status, value, buffer, trace_info);
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
