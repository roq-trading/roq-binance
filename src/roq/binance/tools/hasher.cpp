/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/tools/hasher.hpp"

#include <fmt/format.h>

#include <cassert>

#include "roq/clock.hpp"

#include "roq/utils/safe_cast.hpp"

#include "roq/core/text/writer.hpp"

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
}

std::string_view Hasher::create_query(
    std::span<std::byte> const &buffer, std::chrono::milliseconds now, std::string_view const &body) {
  core::text::Writer writer{buffer};
  writer.write("?timestamp="sv).write(now.count());
  mac_.clear();
  auto tmp = static_cast<std::string_view>(writer).substr(1);
  mac_.update(tmp);
  if (!std::empty(body))
    mac_.update(body);
  auto digest = mac_.final(digest_);
  writer.write("&signature="sv).write(core::codec::Hex{digest_});
  return writer.finish();
}

}  // namespace tools
}  // namespace binance
}  // namespace roq
