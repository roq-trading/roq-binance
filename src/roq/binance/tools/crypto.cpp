/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/tools/crypto.hpp"

#include <fmt/format.h>

#include <cassert>

#include "roq/logging.hpp"

#include "roq/utils/codec/base64.hpp"
#include "roq/utils/codec/url.hpp"

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
auto create_headers_helper_new(auto &key) {
  return fmt::format("X-MBX-APIKEY: {}\r\n"sv, key);
}

auto create_headers_helper_old(auto &key) {
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
auto create_mac(auto &secret, auto margin_mode, auto &secret_2) {
  using result_type = std::remove_cvref_t<R>;
  if (margin_mode == MarginMode::PORTFOLIO) {
    if (!std::empty(secret_2)) {
      log::fatal("Unexpected: secret_2 is now allowed in PORTFOLIO mode"sv);
    }
    return result_type{secret};
  }
  return result_type::create(std::empty(secret_2) ? secret : secret_2, false);
}
}  // namespace

// === IMPLEMENTATION ===

Crypto::Crypto(
    std::string_view const &key, std::string_view const &secret, MarginMode margin_mode, std::string_view const &key_2, std::string_view const &secret_2)
    : key_{key}, headers_new_{create_headers_helper_new(key)}, headers_old_{create_headers_helper_old(std::empty(key_2) ? key : key_2)},
      pkey_{create_ed25519<decltype(pkey_)>(secret, margin_mode)}, mac_{create_mac<decltype(mac_)>(secret, margin_mode, secret_2)} {
  if (std::empty(pkey_) && std::empty(mac_)) {
    log::fatal("The secret must be valid ED25519 or HMAC_SHA256"sv);
  }
}

// ed25519

std::string_view Crypto::create_session_logon_signature(std::string &buffer, std::chrono::milliseconds now_utc) {
  auto payload = fmt::format("apiKey={}&timestamp={}"sv, key_, now_utc.count());
  digest_.clear();
  context_.reset();
  pkey_.sign(digest_, payload, context_);
  utils::codec::Base64::encode(buffer, digest_, false, false);
  return buffer;
}

std::string_view Crypto::create_rest_signature_body_new(std::span<std::byte> const &buffer, std::chrono::milliseconds now_utc, std::string_view const &body) {
  digest_.clear();
  context_.reset();
  pkey_.sign(digest_, body, context_);
  std::string buffer_2;
  utils::codec::Base64::encode(buffer_2, digest_, false, false);
  std::string buffer_3;
  utils::codec::URL::encode(buffer_3, buffer_2);
  utils::text::Writer writer{buffer};
  writer.write("?"sv).write(body);
  writer.write("&signature="sv).write(buffer_3);
  return writer.finish();
}

// classic

std::string_view Crypto::create_rest_signature_old(std::span<std::byte> const &buffer, std::chrono::milliseconds now_utc) {
  utils::text::Writer writer{buffer};
  writer.write("?timestamp="sv).write(now_utc.count());
  mac_.clear();
  auto tmp = static_cast<std::string_view>(writer).substr(1);
  mac_.update(tmp);
  auto digest = mac_.final(digest_);
  writer.write("&signature="sv).write(utils::codec::Hex{digest});
  return writer.finish();
}

std::string_view Crypto::create_rest_signature_old_body(std::span<std::byte> const &buffer, std::chrono::milliseconds now_utc, std::string_view const &body) {
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

std::string Crypto::create_rest_signature_old_query(std::chrono::milliseconds now_utc, std::string_view const &query) {
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
