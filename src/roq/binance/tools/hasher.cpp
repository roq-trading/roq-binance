/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/tools/hasher.h"

#include <fmt/format.h>

#include <array>
#include <cassert>

#include "roq/literals.h"

#include "roq/core/clock.h"

#include "roq/core/binascii/hex.h"

#include "roq/core/crypto/hmac.h"

using namespace roq::literals;

namespace roq {
namespace binance {
namespace tools {

Hasher::Hasher(const std::string_view &key, const std::string_view &secret)
    : key_(key), hmac_(secret) {
}

std::string Hasher::create_query() {
  auto now = core::get_realtime_clock();
  auto [timestamp, signature] = create_signature(now);
  return fmt::format("?{}&signature={}"_sv, timestamp, signature);
}

std::string Hasher::create_headers() const {
  return fmt::format("X-MBX-APIKEY: {}\r\n"_sv, key_);
}

std::pair<std::string, std::string> Hasher::create_signature(std::chrono::nanoseconds now) {
  auto timestamp = fmt::format(
      "timestamp={}"_sv, std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
  hmac_.clear();
  hmac_.update(timestamp);
  std::array<char, 32> buffer;
  auto length = hmac_.digest(buffer);
  assert(length == buffer.size());
  auto signature = core::binascii::Hex::encode(buffer);
  return std::make_pair(timestamp, signature);
}

}  // namespace tools
}  // namespace binance
}  // namespace roq
