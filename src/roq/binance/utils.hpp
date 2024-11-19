/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <string>

namespace roq {
namespace binance {

inline auto normalized_symbol(auto const &value) {
  std::string tmp{value};
  std::transform(std::begin(tmp), std::end(tmp), std::begin(tmp), [](auto c) { return std::tolower(c); });
  return tmp;
};

}  // namespace binance
}  // namespace roq
