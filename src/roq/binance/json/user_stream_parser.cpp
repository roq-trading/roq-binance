/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/json/user_stream_parser.hpp"

#include "roq/logging.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/event_type.hpp"
#include "roq/binance/json/user_stream.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

// === CONSTANTS ===

namespace {
auto const EVENT_TYPE = "e"sv;
}

// === HELPERS ===

namespace {
template <typename T>
void dispatch_helper(auto &handler, auto &message, auto &buffer_stack, auto &trace_info) {
  T obj{message, buffer_stack};
  create_trace_and_dispatch(handler, trace_info, obj);
}

bool try_dispatch(auto &handler, auto &message, auto &buffer_stack, auto event_type, auto &trace_info, auto allow_unknown_event_types) {
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
    case TRADE:
    case _24HR_MINI_TICKER:
    case BOOK_TICKER:
    case DEPTH_UPDATE:
      log::fatal("Unexpected"sv);
      break;
    case OUTBOUND_ACCOUNT_POSITION:
      dispatch_helper<OutboundAccountPosition>(handler, message, buffer_stack, trace_info);
      return true;
    case BALANCE_UPDATE:
      dispatch_helper<BalanceUpdate>(handler, message, buffer_stack, trace_info);
      return true;
    case EXECUTION_REPORT:
      dispatch_helper<ExecutionReport>(handler, message, buffer_stack, trace_info);
      return true;
    case LIST_STATUS:
      dispatch_helper<ListStatus>(handler, message, buffer_stack, trace_info);
      return true;
    case OPEN_ORDER_LOSS:
      // dispatch_helper<OpenOrderLoss>(handler, message, buffer_stack, trace_info);
      return true;
    case ORDER_TRADE_UPDATE:
      // dispatch_helper<OrderTradeUpdate>(handler, message, buffer_stack, trace_info);
      return true;
  }
  log::fatal(R"(Unexpected: message="{}")"sv, message);
}

}  // namespace

// === IMPLEMENTATION ===

bool UserStreamParser::dispatch(
    UserStreamParser::Handler &handler,
    std::string_view const &message,
    core::json::BufferStack &buffer_stack,
    TraceInfo const &trace_info,
    bool allow_unknown_event_types) {
  UserStream user_stream{message, buffer_stack};
  auto &data = user_stream.data;
  core::json::Parser parser{data};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key != EVENT_TYPE) {
      continue;
    }
    EventType event_type{value};
    if (try_dispatch(handler, data, buffer_stack, event_type, trace_info, allow_unknown_event_types)) {
      return true;
    }
    break;
  }
  return false;
}

bool UserStreamParser::dispatch_papi(
    UserStreamParser::Handler &handler,
    std::string_view const &message,
    core::json::BufferStack &buffer_stack,
    TraceInfo const &trace_info,
    bool allow_unknown_event_types) {
  core::json::Parser parser{message};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key != EVENT_TYPE) {
      continue;
    }
    EventType event_type{value};
    if (try_dispatch(handler, message, buffer_stack, event_type, trace_info, allow_unknown_event_types)) {
      return true;
    }
    break;
  }
  return false;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
