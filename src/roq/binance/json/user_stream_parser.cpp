/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/json/user_stream_parser.hpp"

#include "roq/logging.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/user_stream.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

void UserStreamParser::dispatch(
    UserStreamParser::Handler &handler,
    const std::string_view &message,
    core::json::Buffer &buffer,
    const TraceInfo &trace_info) {
  // XXX HANS this is bad... 3 levels of parsing
  // XXX HANS buffer will not be used for first iteration
  auto user_stream = core::json::Parser::create<UserStream>(message, buffer);
  auto &data = user_stream.data;
  core::json::Parser parser(data);
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key.compare("e"sv) != 0)
      continue;
    EventType event_type(value);
    if (try_dispatch(handler, data, buffer, event_type, trace_info))
      return;
    break;
  }
  log::warn(R"(message="{}")"sv, message);
  log::fatal("Unexpected"sv);
}

bool UserStreamParser::try_dispatch(
    UserStreamParser::Handler &handler,
    const std::string_view &message,
    core::json::Buffer &buffer,
    EventType event_type,
    const TraceInfo &trace_info) {
  switch (event_type) {
    using enum EventType::type_t;
    case UNDEFINED:
    case UNKNOWN:
    case AGG_TRADE:
    case TRADE:
    case _24HR_MINI_TICKER:
    case BOOK_TICKER:
    case DEPTH_UPDATE:
      log::fatal("Unexpected"sv);
      break;
    case OUTBOUND_ACCOUNT_POSITION: {
      const auto outbound_account_position =
          core::json::Parser::create<OutboundAccountPosition>(message, buffer);
      Trace event(trace_info, outbound_account_position);
      handler(event);
      break;
    }
    case BALANCE_UPDATE: {
      const auto balance_update = core::json::Parser::create<BalanceUpdate>(message);
      Trace event(trace_info, balance_update);
      handler(event);
      break;
    }
    case EXECUTION_REPORT: {
      const auto execution_report = core::json::Parser::create<ExecutionReport>(message);
      Trace event(trace_info, execution_report);
      handler(event);
      break;
    }
    case LIST_STATUS: {
      const auto list_status = core::json::Parser::create<ListStatus>(message, buffer);
      Trace event(trace_info, list_status);
      handler(event);
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
