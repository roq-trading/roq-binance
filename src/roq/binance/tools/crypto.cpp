/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/tools/crypto.hpp"

#include <fmt/format.h>

#include <cassert>

#include "roq/logging.hpp"

#include "roq/utils/codec/base64.hpp"

#include "roq/utils/text/writer.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace tools {

// === VALIDATION ===

namespace {
static_assert(Crypto::QUERY_BUFFER_LENGTH % 64 == 0);
}  // namespace

// === HELPERS ===

namespace {
auto create_headers_helper(auto &key) {
  return fmt::format("X-MBX-APIKEY: {}\r\n"sv, key);
}

template <typename R>
auto create_ed25519(auto &secret, auto margin_mode) {
  using result_type = std::remove_cvref_t<R>;
  if (margin_mode != MarginMode::PORTFOLIO) {
    return result_type{secret};
  }
  return result_type{};
}

template <typename R>
auto create_hmac_sha256(auto &secret, auto margin_mode) {
  using result_type = std::remove_cvref_t<R>;
  if (margin_mode == MarginMode::PORTFOLIO) {
    return result_type{secret};
  }
  return result_type{};
}
}  // namespace

// === IMPLEMENTATION ===

Crypto::Crypto(std::string_view const &key, std::string_view const &secret, MarginMode margin_mode)
    : key_{key}, headers_{create_headers_helper(key_)}, pkey_{create_ed25519<decltype(pkey_)>(secret, margin_mode)},
      mac_{create_hmac_sha256<decltype(mac_)>(secret, margin_mode)} {
}

// ed25519

std::string_view Crypto::create_session_logon_signature(std::string &buffer, std::chrono::milliseconds timestamp) {
  auto payload = fmt::format("apiKey={}&timestamp={}"sv, key_, timestamp.count());
  digest_.clear();
  context_.reset();
  pkey_.sign(digest_, payload, context_);
  utils::codec::Base64::encode(buffer, digest_, false, false);
  return buffer;
}

// classic

std::string_view Crypto::create_rest_signature(std::span<std::byte> const &buffer, std::chrono::milliseconds now_utc) {
  utils::text::Writer writer{buffer};
  writer.write("?timestamp="sv).write(now_utc.count());
  mac_.clear();
  auto tmp = static_cast<std::string_view>(writer).substr(1);
  mac_.update(tmp);
  auto digest = mac_.final(digest_);
  writer.write("&signature="sv).write(utils::codec::Hex{digest});
  return writer.finish();
}

std::string_view Crypto::create_rest_signature_body(std::span<std::byte> const &buffer, std::chrono::milliseconds now_utc, std::string_view const &body) {
  utils::text::Writer writer{buffer};
  writer.write("?timestamp="sv).write(now_utc.count());
  mac_.clear();
  auto tmp = static_cast<std::string_view>(writer).substr(1);
  mac_.update(tmp);
  assert(!std::empty(body));
  mac_.update(body);
  auto digest = mac_.final(digest_);
  writer.write("&signature="sv).write(utils::codec::Hex{digest});
  return writer.finish();
}

std::string Crypto::create_rest_signature_query(std::chrono::milliseconds now_utc, std::string_view const &query) {
  auto tmp = fmt::format("{}&timestamp={}"sv, query, now_utc.count());
  mac_.clear();
  mac_.update(tmp);
  auto digest = mac_.final(digest_);
  std::string signature;
  utils::codec::Hex::encode(signature, digest);
  return fmt::format("?{}&signature={}"sv, tmp, signature);
}

}  // namespace tools
}  // namespace binance
}  // namespace roq
