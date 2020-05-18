/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/json/parser.h"

#include "roq/compat.h"

#include "roq/binance/json/event_type.h"

#include "roq/logging.h"

namespace roq {
namespace binance {
namespace json {

namespace {
static EventType find_event_type(const auto& message) {
  constexpr std::string_view EVENT_TYPE = "e";
  core::json::Parser parser(message);
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::object_t>(root)) {
    if (key.compare(EVENT_TYPE) == 0) {
      return EventType(value);
    }
  }
  return EventType::UNDEFINED;
}
}  // namespace

void Parser::dispatch(
    Parser::Handler& handler,
    const std::string_view& message,
    core::json::Buffer& buffer) {
  auto event_type = find_event_type(message);
  core::json::Parser parser(message);
  auto root = parser.root();
  switch (event_type) {
    case EventType::UNDEFINED:
    case EventType::UNKNOWN:
      LOG(WARNING)(FMT_STRING("message=\"{}\""), message);
      LOG(FATAL)("Unexpected");
      break;
    case EventType::TRADE: {
      Trade trade(root);
      handler(trade);
      break;
    }
    case EventType::_24HR_TICKER: {
      Ticker ticker(root);
      handler(ticker);
      break;
    }
    case EventType::DEPTH_UPDATE: {
      DepthUpdate depth_update(
          root,
          buffer);
      handler(depth_update);
      break;
    }
    case EventType::ACCOUNT_UPDATE:
    case EventType::BALANCE_UPDATE:
    case EventType::EXECUTION_REPORT:
      break;
  }
}

}  // namespace json
}  // namespace binance
}  // namespace roq
