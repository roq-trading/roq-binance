/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <chrono>

#include "roq/core/utility.h"

#include "roq/core/json/parser.h"

#include "roq/core/charconv/datetime.h"

namespace roq {
namespace binance {
namespace json {

template <typename T>
inline void update(
    T& result,
    const core::json::value_t& value) {
  result = core::json::get<T>(value);
}

template <>
inline void update(
    std::chrono::nanoseconds& result,
    const core::json::value_t& value) {
  return std::visit(
      overloaded {
        [&](const core::json::null_t&) {
          result = std::chrono::nanoseconds {};
        },
        [](bool) {
          throw std::bad_cast();
        },
        [&](int64_t value) {
          result = std::chrono::nanoseconds {
            static_cast<uint64_t>(value * int64_t{1000000000})
          };
        },
        [&](double value) {
          result = std::chrono::nanoseconds {
            static_cast<uint64_t>(value * 1.0e9)
          };
        },
        [&](const std::string_view& value) {
          result = core::charconv::to_datetime(value);
        },
        [](const core::json::object_t&) {
          throw std::bad_cast();
        },
        [](const core::json::array_t&) {
          throw std::bad_cast();
        },
      },
      value);
}

}  // namespace json
}  // namespace binance
}  // namespace roq
