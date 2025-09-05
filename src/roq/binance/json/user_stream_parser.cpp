/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/json/user_stream_parser.hpp"

#include "roq/logging.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/user_stream.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

bool UserStreamParser::dispatch(
    UserStreamParser::Handler &handler,
    std::string_view const &message,
    core::json::BufferStack &buffer_stack,
    TraceInfo const &trace_info,
    bool continue_with_unknown_event_type) {
  // XXX this is bad... 3 levels of parsing
  // XXX buffer_stack will not be used for first iteration
  UserStream user_stream{message, buffer_stack};
  auto &data = user_stream.data;
  core::json::Parser parser{data};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key != "e"sv) {
      continue;
    }
    EventType event_type{value};
    if (try_dispatch(handler, data, buffer_stack, event_type, trace_info, continue_with_unknown_event_type)) {
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
    bool continue_with_unknown_event_type) {
  core::json::Parser parser{message};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key != "e"sv) {
      continue;
    }
    EventType event_type{value};
    if (try_dispatch(handler, message, buffer_stack, event_type, trace_info, continue_with_unknown_event_type)) {
      return true;
    }
    break;
  }
  return false;
}

bool UserStreamParser::try_dispatch(
    UserStreamParser::Handler &handler,
    std::string_view const &message,
    core::json::BufferStack &buffer_stack,
    EventType event_type,
    TraceInfo const &trace_info,
    bool continue_with_unknown_event_type) {
  switch (event_type) {
    using enum EventType::type_t;
    case UNKNOWN_INTERNAL:
      if (!continue_with_unknown_event_type) {
        log::fatal("Unexpected"sv);
      }
      return false;
    case UNDEFINED_INTERNAL:
    case AGG_TRADE:
    case TRADE:
    case _24HR_MINI_TICKER:
    case BOOK_TICKER:
    case DEPTH_UPDATE:
      log::fatal("Unexpected"sv);
      break;
    case OUTBOUND_ACCOUNT_POSITION: {
      OutboundAccountPosition outbound_account_position{message, buffer_stack};
      Trace event{trace_info, outbound_account_position};
      handler(event);
      break;
    }
    case BALANCE_UPDATE: {
      BalanceUpdate balance_update{message};
      Trace event{trace_info, balance_update};
      handler(event);
      break;
    }
    case EXECUTION_REPORT: {
      ExecutionReport execution_report{message};
      Trace event{trace_info, execution_report};
      handler(event);
      break;
    }
    case LIST_STATUS: {
      ListStatus list_status{message, buffer_stack};
      Trace event{trace_info, list_status};
      handler(event);
      break;
    }
    case OPEN_ORDER_LOSS: {
      // OpenOrderLoss open_order_loss{message, buffer_stack};
      // Trace event{trace_info, open_order_loss};
      // handler(event);
      break;
    }
    default:
      return false;
  }
  return true;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
