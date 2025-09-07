/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/json/wsapi_parser_2.hpp"

#include "roq/logging.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/wsapi_type.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

// === CONSTANTS ===

namespace {
auto const FIELD_ID = "id"sv;
}

// === HELPERS ===

namespace {
template <typename T, typename... Args>
void dispatch_helper(auto &handler, auto &value, auto &buffer_stack, auto &trace_info, Args &&...args) {
  T obj{value, buffer_stack};
  create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
}

auto get_request_from_id(auto &message) -> WSAPIRequest {
  core::json::Parser parser{message};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key == FIELD_ID) {
      auto id = core::json::get<std::string_view>(value);
      return WSAPIRequest::decode(id);
    }
  }
  return {};
}
}  // namespace

bool WSAPIParser2::dispatch(
    WSAPIParser2::Handler &handler,
    std::string_view const &message,
    core::json::BufferStack &buffer_stack,
    TraceInfo const &trace_info,
    bool allow_unknown_event_types) {
  auto request = get_request_from_id(message);
  switch (request.type) {
    using enum WSAPIType::type_t;
    case UNDEFINED_INTERNAL:
      break;
    case UNKNOWN_INTERNAL:
      if (allow_unknown_event_types) {
        return false;
      }
      break;
    case LISTEN_KEY_CREATE:
      dispatch_helper<WSAPIListenKey>(handler, message, buffer_stack, trace_info, request);
      return true;
    case LISTEN_KEY_PING:
      // note! we drop
      return true;
    case ACCOUNT_STATUS:
      dispatch_helper<WSAPIAccount>(handler, message, buffer_stack, trace_info, request);
      return true;
    case OPEN_ORDERS_STATUS:
      dispatch_helper<WSAPIOpenOrders>(handler, message, buffer_stack, trace_info, request);
      return true;
    case MY_TRADES:
      dispatch_helper<WSAPITrades>(handler, message, buffer_stack, trace_info, request);
      return true;
    case OPEN_ORDERS_CANCEL_ALL:
      dispatch_helper<WSAPICancelOpenOrders>(handler, message, buffer_stack, trace_info, request);
      return true;
    case ORDER_PLACE:
      dispatch_helper<WSAPIOrderPlace>(handler, message, buffer_stack, trace_info, request);
      return true;
    case ORDER_CANCEL:
      dispatch_helper<WSAPICancelOrder>(handler, message, buffer_stack, trace_info, request);
      return true;
    case ORDER_CANCEL_REPLACE:
      dispatch_helper<WSAPICancelReplaceOrder>(handler, message, buffer_stack, trace_info, request);
      return true;
  }
  log::fatal(R"(Unexpected: message="{}")"sv, message);
}

}  // namespace json
}  // namespace binance
}  // namespace roq
