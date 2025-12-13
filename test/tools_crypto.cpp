/* Copyright (c) 2017-2026, Hans Erik Thrane */

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
auto const KEY = "IUA8RM0EJObe4X7UWxy74jAp6s2TJL2flZgDoxzx5AU3ws1I81Gr8ID3MVwfnYVD"sv;
auto const SECRET = "MC4CAQAwBQYDK2VwBCIEIKpUYyNZ0pOcbuqxHgXnmrV2veFbP/dEJosCjJXalt22"sv;
}  // namespace

// === IMPLEMENTATION ===

TEST_CASE("simple_ed25519", "[tools_crypto]") {
  tools::Crypto crypto{KEY, SECRET, MarginMode::UNDEFINED};
  std::string buffer;
  auto now = 1674303865s;
  auto query = crypto.create_session_logon_signature(buffer, now);
  auto expected = "imcEenTd/ElJ8eQ0n7Hx2QGTd4lX+/uJFRG/8HqdzOkMhyyFG8h5MtjqucuBPnTFbBiAkVXqt5QvVK0r4kFxDA=="sv;
  CHECK(query == expected);
}
