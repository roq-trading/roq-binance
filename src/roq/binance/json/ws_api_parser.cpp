/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/json/ws_api_parser.hpp"

#include "roq/logging.hpp"

#include "roq/core/json/parser.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

namespace {
bool try_dispatch(auto &handler, auto &id, auto status, auto &value, auto &buffer, auto &trace_info) {
  auto listen_key = ListenKey{value, buffer};
  create_trace_and_dispatch(handler, trace_info, listen_key);
  return true;
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
          return try_dispatch(handler, id, status, value, buffer, trace_info);
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
