/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <array>
#include <string>
#include <string_view>

#include "roq/core/mac/hmac.hpp"

namespace roq {
namespace binance {
namespace tools {

struct Hasher final {
  Hasher(std::string_view const &key, std::string_view const &secret);

  Hasher(Hasher &&) = delete;
  Hasher(Hasher const &) = delete;

  std::string_view create_query(std::string_view const &body);

  std::string_view create_headers() const { return headers_; }

 private:
  using MAC = core::mac::HMAC<core::hash::SHA256>;
  using Digest = std::array<std::byte, MAC::DIGEST_LENGTH>;

  std::string const key_;
  MAC mac_;
  Digest digest_;
  std::string const headers_;
  std::string signature_;
  std::string result_;
};

}  // namespace tools
}  // namespace binance
}  // namespace roq
