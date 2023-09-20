/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/ping.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_ping_simple", "[json_ping]") {
  auto message = R"({)"
                 R"("serverTime":1634180186435)"
                 R"(})";
  auto obj = core::json::Parser::create<json::Ping>(message);
  CHECK(obj.server_time == 1634180186435ms);
}
