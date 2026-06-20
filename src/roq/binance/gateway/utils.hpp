/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <algorithm>
#include <string>

namespace roq {
namespace binance {
namespace gateway {

inline auto normalized_symbol(auto const &value) {
  std::string tmp{value};
  std::ranges::transform(tmp, std::begin(tmp), [](auto item) { return std::tolower(item); });
  return tmp;
};

}  // namespace gateway
}  // namespace binance
}  // namespace roq
