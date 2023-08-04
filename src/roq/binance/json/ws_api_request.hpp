/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include <string_view>
#include <vector>

#include "roq/binance/json/ws_api_type.hpp"

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

  static std::string_view encode(std::vector<char> &buffer, WSAPIRequest const &);
  static WSAPIRequest decode(std::string_view const &buffer);
};

}  // namespace json
}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::json::WSAPIRequest> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::binance::json::WSAPIRequest const &value, Context &context) const {
    using namespace std::literals;
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(sequence="{}", )"
        R"(type={}, )"
        R"(user_id={}, )"
        R"(order_id={}, )"
        R"(version={}, )"
        R"(order_id_2={})"
        R"(}})"_cf,
        value.sequence,
        value.type,
        value.user_id,
        value.order_id,
        value.version,
        value.order_id_2);
  }
};
