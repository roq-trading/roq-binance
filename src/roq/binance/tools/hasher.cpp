/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/tools/hasher.h"

#include <fmt/format.h>

#include <array>
#include <cassert>

#include "roq/literals.h"

#include "roq/utils/safe_cast.h"

#include "roq/core/clock.h"

#include "roq/core/binascii/hex.h"

#include "roq/core/crypto/hmac.h"

using namespace roq::literals;

namespace roq {
namespace binance {
namespace tools {

Hasher::Hasher(const std::string_view &key, const std::string_view &secret)
    : key_(key), hmac_(secret), headers_(fmt::format("X-MBX-APIKEY: {}\r\n"_sv, key_)) {
}

std::string Hasher::create_query(const std::string_view &body) {
  std::chrono::milliseconds now = utils::safe_cast(core::get_realtime_clock());
  auto timestamp = fmt::format("timestamp={}"_sv, now.count());
  hmac_.clear();
  hmac_.update(timestamp);
  if (!std::empty(body))
    hmac_.update(body);
  std::array<char, 32> buffer;
  auto length = hmac_.digest(buffer);
  assert(length == buffer.size());
  auto signature = core::binascii::Hex::encode(buffer);
  return fmt::format("?{}&signature={}"_sv, timestamp, signature);
}

}  // namespace tools
}  // namespace binance
}  // namespace roq
