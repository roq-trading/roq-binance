/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/tools/crypto.hpp"

#include <fmt/format.h>

#include <cassert>

#include "roq/clock.hpp"

#include "roq/utils/safe_cast.hpp"

#include "roq/utils/codec/base64.hpp"

#include "roq/utils/text/writer.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace tools {

// === HELPERS ===

namespace {
auto create_headers_helper(auto const &key) {
  return fmt::format("X-MBX-APIKEY: {}\r\n"sv, key);
}
}  // namespace

// === IMPLEMENTATION ===

Crypto::Crypto(std::string_view const &key, std::string_view const &secret) : key_{key}, headers_{create_headers_helper(key_)}, pkey_{secret} {
}

std::string_view Crypto::create_session_logon_signature(std::string &buffer, std::chrono::milliseconds timestamp) {
  auto payload = fmt::format("apiKey={}&timestamp={}"sv, key_, timestamp.count());
  digest_.clear();
  context_.reset();
  pkey_.sign(digest_, payload, context_);
  utils::codec::Base64::encode(buffer, digest_, false, false);
  return buffer;
}

}  // namespace tools
}  // namespace binance
}  // namespace roq
