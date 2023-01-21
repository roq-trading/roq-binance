/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/tools/hasher.hpp"

#include <fmt/format.h>

#include <cassert>

#include "roq/clock.hpp"

#include "roq/utils/safe_cast.hpp"

#include "roq/core/codec/hex.hpp"

using namespace std::literals;

using namespace fmt::literals;

namespace roq {
namespace binance {
namespace tools {

// === VALIDATION ===

namespace {
static_assert(Hasher::QUERY_BUFFER_LENGTH % 64 == 0);
}  // namespace

// === HELPERS ===

namespace {
auto create_headers_helper(auto const &key) {
  return fmt::format("X-MBX-APIKEY: {}\r\n"_cf, key);
}
}  // namespace

// === IMPLEMENTATION ===

Hasher::Hasher(std::string_view const &key, std::string_view const &secret)
    : key_{key}, mac_{secret}, headers_{create_headers_helper(key_)} {
  result_.reserve(64);
}

std::string_view Hasher::create_query(
    core::Message<char> &buffer, std::chrono::milliseconds now, std::string_view const &body) {
  auto timestamp = fmt::format("timestamp={}"_cf, now.count());
  mac_.clear();
  mac_.update(timestamp);
  if (!std::empty(body))
    mac_.update(body);
  auto digest = mac_.final(digest_);
  core::codec::Hex::encode(signature_, digest);
  result_.clear();
  fmt::format_to(std::back_inserter(result_), "?{}&signature={}"_cf, timestamp, signature_);
  return result_;
}

}  // namespace tools
}  // namespace binance
}  // namespace roq
