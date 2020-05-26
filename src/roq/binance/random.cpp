/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/random.h"

#include <fmt/format.h>

#include <cassert>

#include "roq/core/binascii/hex.h"

#include "roq/core/crypto/hmac.h"

namespace roq {
namespace binance {

Random::Random(
    const std::string_view& key,
    const std::string_view& secret)
    : _key(key),
      _hmac(
          secret.data(),
          secret.length()) {
}

std::string Random::create_signature(
    const std::string_view& timestamp) {
  _hmac.clear();
  _hmac.update(timestamp);
  std::array<char, 32> buffer;
  auto length = _hmac.digest(
      buffer.data(),
      buffer.size());
  assert(length == buffer.size());
  return core::binascii::Hex::encode(
      buffer.data(),
      buffer.size());
}

}  // namespace binance
}  // namespace roq
