/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include <string>
#include <string_view>

#include "roq/binance/json/wsapi_type.hpp"

namespace roq {
namespace binance {
namespace json {

struct WSAPIRequest final {
  uint32_t sequence = {};
  json::WSAPIType type = {};
  uint8_t user_id = {};
  uint64_t order_id = {};
  uint32_t version = {};
  uint64_t order_id_2 = {};

  static std::string_view encode(std::string &buffer, WSAPIRequest const &);
  static WSAPIRequest decode(std::string_view const &buffer);
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::WSAPIRequest> {
  constexpr auto parse(format_parse_context &context) { return std::begin(context); }
  auto format(roq::binance::json::WSAPIRequest const &value, format_context &context) const {
    using namespace std::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(sequence="{}", )"
        R"(type={}, )"
        R"(user_id={}, )"
        R"(order_id={}, )"
        R"(version={}, )"
        R"(order_id_2={})"
        R"(}})"sv,
        value.sequence,
        value.type,
        value.user_id,
        value.order_id,
        value.version,
        value.order_id_2);
  }
};
