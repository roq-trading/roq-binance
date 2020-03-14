/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/json/parser.h"

namespace roq {
namespace binance {
namespace json {

void Parser::dispatch(
    Handler& handler,
    const std::string_view& message,
    core::json::Buffer& buffer) {
  (void) handler;  // avoid warning
  (void) message;  // avoid warning
  (void) buffer;  // avoid warning
}

}  // namespace json
}  // namespace binance
}  // namespace roq
