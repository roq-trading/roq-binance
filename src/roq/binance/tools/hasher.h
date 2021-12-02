/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "roq/core/crypto/hmac.h"

namespace roq {
namespace binance {
namespace tools {

class Hasher final {
 public:
  Hasher(const std::string_view &key, const std::string_view &secret);

  Hasher(Hasher &&) = delete;
  Hasher(const Hasher &) = delete;

  std::string create_query(const std::string_view &body);

  std::string_view create_headers() const { return headers_; }

 private:
  const std::string key_;
  core::crypto::HMAC_SHA256 hmac_;
  const std::string headers_;
};

}  // namespace tools
}  // namespace binance
}  // namespace roq
