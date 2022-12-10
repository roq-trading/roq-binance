/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "roq/core/crypto/hmac_sha256.hpp"

namespace roq {
namespace binance {
namespace tools {

class Hasher final {
 public:
  Hasher(std::string_view const &key, std::string_view const &secret);

  Hasher(Hasher &&) = delete;
  Hasher(Hasher const &) = delete;

  std::string_view create_query(std::string_view const &body);

  std::string_view create_headers() const { return headers_; }

 private:
  std::string const key_;
  core::crypto::HMAC_SHA256 hmac_;
  std::string const headers_;
  alignas(32) std::array<char, 32> buffer_;
  std::string signature_;
  std::string result_;
};

}  // namespace tools
}  // namespace binance
}  // namespace roq
