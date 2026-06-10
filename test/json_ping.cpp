/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"

#include "roq/binance/protocol/json/ping.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::Ping;

TEST_CASE("json_ping_simple", "[json_ping]") {
  auto message = R"({)"
                 R"("serverTime":1634180186435)"
                 R"(})"sv;
  auto helper = [&](value_type &obj) { CHECK(obj.server_time == 1634180186435ms); };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}
