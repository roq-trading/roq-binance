/* Copyright (c) 2017-2025, Hans Erik Thrane */

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
    if (key == "id"sv) {
      auto id = core::json::get<std::string_view>(value);
      return WSAPIRequest::decode(id);
    }
  }
  return {};
}

template <typename T>
bool dispatch_helper(auto &handler, auto &message, auto &buffer, auto &trace_info, auto &request) {
  T obj{message, buffer};
  create_trace_and_dispatch(handler, trace_info, obj, request);
  return true;
}
}  // namespace

bool WSAPIParser2::dispatch(WSAPIParser2::Handler &handler, std::string_view const &message, std::span<std::byte> const &buffer, TraceInfo const &trace_info) {
  auto request = get_request(message);
  switch (request.type) {
    using enum WSAPIType::type_t;
    case UNDEFINED_INTERNAL:
    case UNKNOWN_INTERNAL:
      break;
    case LISTEN_KEY_CREATE:
      return dispatch_helper<WSAPIListenKey>(handler, message, buffer, trace_info, request);
    case LISTEN_KEY_PING:
      return true;  // note!
    case ACCOUNT_STATUS:
      return dispatch_helper<WSAPIAccount>(handler, message, buffer, trace_info, request);
    case OPEN_ORDERS_STATUS:
      return dispatch_helper<WSAPIOpenOrders>(handler, message, buffer, trace_info, request);
    case MY_TRADES:
      return dispatch_helper<WSAPITrades>(handler, message, buffer, trace_info, request);
    case OPEN_ORDERS_CANCEL_ALL:
      return dispatch_helper<WSAPICancelOpenOrders>(handler, message, buffer, trace_info, request);
    case ORDER_PLACE:
      return dispatch_helper<WSAPIOrderPlace>(handler, message, buffer, trace_info, request);
    case ORDER_CANCEL:
      return dispatch_helper<WSAPICancelOrder>(handler, message, buffer, trace_info, request);
    case ORDER_CANCEL_REPLACE:
      return dispatch_helper<WSAPICancelReplaceOrder>(handler, message, buffer, trace_info, request);
  }
  return false;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
