/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/json/wsapi_parser_2.hpp"

#include "roq/logging.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/wsapi_type.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

// === HELPERS ===

namespace {
template <typename T, typename... Args>
void dispatch_helper(auto &handler, auto &value, auto &buffer_stack, auto &trace_info, Args &&...args) {
  T obj{value, buffer_stack};
  create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
}

bool dispatch_response(auto &handler, auto &message, auto &buffer_stack, auto &trace_info, auto allow_unknown_event_types, auto &request) {
  switch (request.type) {
    using enum WSAPIType::type_t;
    case UNDEFINED_INTERNAL:
      break;
    case UNKNOWN_INTERNAL:
      if (allow_unknown_event_types) {
        return false;
      }
      break;
    case SESSION_LOGON:
      dispatch_helper<WSAPISessionLogon>(handler, message, buffer_stack, trace_info, request);
      return true;
    case USER_DATA_STREAM_SUBSCRIBE:
      dispatch_helper<WSAPIUserDataStreamSubscribe>(handler, message, buffer_stack, trace_info, request);
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
  }
  log::fatal(R"(Unexpected: message="{}")"sv, message);
}

bool dispatch_event(auto &handler, auto &message, auto &buffer_stack, auto &trace_info, auto allow_unknown_event_types, auto &event_type) {
  switch (event_type) {
    using enum EventType::type_t;
    case UNDEFINED_INTERNAL:
      break;
    case UNKNOWN_INTERNAL:
      if (allow_unknown_event_types) {
        return false;
      }
      break;
    case AGG_TRADE:
      break;
    case TRADE:
      break;
    case _24HR_MINI_TICKER:
      break;
    case BOOK_TICKER:
      break;
    case DEPTH_UPDATE:
      break;
    case OUTBOUND_ACCOUNT_POSITION:
      dispatch_helper<WSAPIOutboundAccountPosition>(handler, message, buffer_stack, trace_info);
      return true;
    case BALANCE_UPDATE:
      dispatch_helper<WSAPIBalanceUpdate>(handler, message, buffer_stack, trace_info);
      return true;
    case EXECUTION_REPORT:
      dispatch_helper<WSAPIExecutionReport>(handler, message, buffer_stack, trace_info);
      return true;
    case LIST_STATUS:
      break;
    case OPEN_ORDER_LOSS:
      break;
    case ORDER_TRADE_UPDATE:
      break;
  }
  log::fatal(R"(Unexpected: message="{}")"sv, message);
}
}  // namespace

bool WSAPIParser2::dispatch(
    WSAPIParser2::Handler &handler,
    std::string_view const &message,
    core::json::BufferStack &buffer_stack,
    TraceInfo const &trace_info,
    bool allow_unknown_event_types) {
  core::json::Parser parser{message};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key == "id"sv) {
      auto id = core::json::get<std::string_view>(value);
      auto request = WSAPIRequest::decode(id);
      return dispatch_response(handler, message, buffer_stack, trace_info, allow_unknown_event_types, request);
    } else if (key == "event"sv) {
      for (auto [k, v] : std::get<core::json::Object>(value)) {
        if (k == "e"sv) {
          EventType event_type{v};
          return dispatch_event(handler, message, buffer_stack, trace_info, allow_unknown_event_types, event_type);
        }
      }
    }
  }
  if (allow_unknown_event_types) {
    return false;
  }
  log::fatal(R"(Unexpected: message="{}")"sv, message);
}

}  // namespace json
}  // namespace binance
}  // namespace roq
