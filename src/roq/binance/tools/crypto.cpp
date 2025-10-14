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

std::string_view Crypto::create_query(std::span<std::byte> const &buffer, std::chrono::milliseconds now, std::string_view const &body) {
  throw RuntimeError{"XXX"sv};
}

std::string_view Crypto::create_ws_api_signature(std::string &buffer, std::string_view const &body) {
  digest_.clear();
  context_.reset();
  pkey_.sign(digest_, body, context_);
  utils::codec::Base64::encode(buffer, digest_, false, false);
}

}  // namespace tools
}  // namespace binance
}  // namespace roq
