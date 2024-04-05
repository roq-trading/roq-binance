/* Copyright (c) 2017-2024, Hans Erik Thrane */

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
    std::span<std::byte> const &buffer,
    TraceInfo const &trace_info) {
  // XXX this is bad... 3 levels of parsing
  // XXX buffer will not be used for first iteration
  UserStream user_stream{message, buffer};
  auto &data = user_stream.data;
  core::json::Parser parser{data};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key.compare("e"sv) != 0)
      continue;
    EventType event_type{value};
    if (try_dispatch(handler, data, buffer, event_type, trace_info))
      return true;
    break;
  }
  return false;
}

bool UserStreamParser::dispatch_papi(
    UserStreamParser::Handler &handler,
    std::string_view const &message,
    std::span<std::byte> const &buffer,
    TraceInfo const &trace_info) {
  core::json::Parser parser{message};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key.compare("e"sv) != 0)
      continue;
    EventType event_type{value};
    if (try_dispatch(handler, message, buffer, event_type, trace_info))
      return true;
    break;
  }
  return false;
}

bool UserStreamParser::try_dispatch(
    UserStreamParser::Handler &handler,
    std::string_view const &message,
    std::span<std::byte> const &buffer,
    EventType event_type,
    TraceInfo const &trace_info) {
  switch (event_type) {
    using enum EventType::type_t;
    case UNDEFINED__:
    case UNKNOWN__:
    case AGG_TRADE:
    case TRADE:
    case _24HR_MINI_TICKER:
    case BOOK_TICKER:
    case DEPTH_UPDATE:
      log::fatal("Unexpected"sv);
      break;
    case OUTBOUND_ACCOUNT_POSITION: {
      OutboundAccountPosition outbound_account_position{message, buffer};
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
      ListStatus list_status{message, buffer};
      Trace event{trace_info, list_status};
      handler(event);
      break;
    }
    case OPEN_ORDER_LOSS: {
      // OpenOrderLoss open_order_loss{message, buffer};
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
