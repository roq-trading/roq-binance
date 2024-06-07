/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <roq/utils/hash/sha256.hpp>

#include "roq/binance/tools/crypto.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

// === CONSTANTS ===

namespace {
auto const KEY = "sSzUA6j8tGDfmLoFrOPhWHY3VeXbC3NrApp94Ci4H4XvcjuCuvOXp8gH89XzMPDe"sv;
auto const SECRET = "tHurnNFWLFkm97xVRqoESdujAiq1ilNjnY52tDej5RilUbTVZXT2YB5eo7txFLHk"sv;
}  // namespace

// === IMPLEMENTATION ===

TEST_CASE("simple", "[tools_crypto]") {
  tools::Crypto crypto{KEY, SECRET};
  std::vector<std::byte> buffer(4096);
  auto now = 1674303865s;
  auto body = "abc=123&def=456"sv;
  auto query = crypto.create_query(buffer, now, body);
  auto expected = "?timestamp=1674303865000&signature=fa3ec135cd0ca6fd1267e30feb51147885209b0ee6c997ace0a6c7694b29736f"sv;
  CHECK(query == expected);
}
