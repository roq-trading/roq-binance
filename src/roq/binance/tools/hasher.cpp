/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/tools/hasher.hpp"

#include <fmt/format.h>

#include <array>
#include <cassert>

#include "roq/clock.hpp"

#include "roq/utils/safe_cast.hpp"

#include "roq/core/binascii/hex.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace tools {

// === HELPERS ===

namespace {
auto create_headers_helper(auto const &key) {
  return fmt::format("X-MBX-APIKEY: {}\r\n"sv, key);
}
}  // namespace

// === IMPLEMENTATION ===

Hasher::Hasher(std::string_view const &key, std::string_view const &secret)
    : key_(key), hmac_(secret), headers_(create_headers_helper(key_)) {
}

std::string Hasher::create_query(std::string_view const &body) {
  auto now = clock::get_realtime<std::chrono::milliseconds>();
  auto timestamp = fmt::format("timestamp={}"sv, now.count());
  hmac_.clear();
  hmac_.update(timestamp);
  if (!std::empty(body))
    hmac_.update(body);
  std::array<char, 32> buffer;
  auto length = hmac_.digest(buffer);
  assert(length == std::size(buffer));
  auto signature = core::binascii::Hex::encode(buffer);
  return fmt::format("?{}&signature={}"sv, timestamp, signature);
}

}  // namespace tools
}  // namespace binance
}  // namespace roq
