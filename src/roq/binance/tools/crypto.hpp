/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "roq/margin_mode.hpp"

#include "roq/utils/signature/context.hpp"
#include "roq/utils/signature/pkey.hpp"

#include "roq/utils/mac/hmac.hpp"

namespace roq {
namespace binance {
namespace tools {

struct Crypto final {
  Crypto(std::string_view const &key, std::string_view const &secret, MarginMode, std::string_view const &key_2, std::string_view const &secret_2);

  Crypto(Crypto &&) = delete;
  Crypto(Crypto const &) = delete;

  std::string_view get_key() const { return key_; }

  // ed25519

  std::string_view get_rest_headers_new() const { return headers_new_; }

  std::string_view create_session_logon_signature(std::string &buffer, std::chrono::milliseconds now_utc);

  std::string_view create_rest_signature_body_new(std::span<std::byte> const &buffer, std::chrono::milliseconds now_utc, std::string_view const &body);

  // classic

  std::string_view get_rest_headers_old() const { return headers_old_; }

  static constexpr auto const QUERY_BUFFER_LENGTH = 512uz;  // note! expected length == 99

  std::string_view create_rest_signature_old(std::span<std::byte> const &buffer, std::chrono::milliseconds now_utc);

  std::string_view create_rest_signature_old_body(std::span<std::byte> const &buffer, std::chrono::milliseconds now_utc, std::string_view const &body);

  std::string create_rest_signature_old_query(std::chrono::milliseconds now_utc, std::string_view const &message);

 private:
  std::string const key_;
  std::string const headers_new_;
  std::string const headers_old_;

  // ed25519
  utils::signature::PKey pkey_;
  utils::signature::Context context_;
  std::vector<std::byte> digest_;

  // classic
  using MAC = utils::mac::HMAC<utils::hash::SHA256>;
  using Digest = std::array<std::byte, MAC::DIGEST_LENGTH>;
  MAC mac_;
  Digest digest_2_;
};

}  // namespace tools
}  // namespace binance
}  // namespace roq
