/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/ping.h"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

TEST(json_ping, simple) {
  auto message = R"({)"
                 R"("serverTime":1634180186435)"
                 R"(})";
  auto obj = core::json::Parser::create<json::Ping>(message);
  EXPECT_EQ(obj.server_time, 1634180186435ms);
}
