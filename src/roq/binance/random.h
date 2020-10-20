/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "roq/core/crypto/hmac.h"

#include "roq/core/http/method.h"

namespace roq {
namespace binance {

class Random final {
 public:
  Random(const std::string_view &key, const std::string_view &secret);

  Random(Random &&) = delete;
  Random(const Random &) = delete;

  std::string create_signature(const std::string_view &timestamp);

 private:
  const std::string _key;
  core::crypto::HMAC_SHA256 _hmac;
};

}  // namespace binance
}  // namespace roq
