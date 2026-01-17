/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "user_stream_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::OutboundAccountPosition;

TEST_CASE("simple", "[json_outbound_account_position]") {
  auto message = R"({)"
                 R"("stream":"sj9ht0LN4uqn6kILaJpcsmZ5q2bjVInmmJPl8PdStouqzwHiwHgbwEaBm1ai",)"
                 R"("data":{)"
                 R"("e":"outboundAccountPosition",)"
                 R"("E":1634285425303,)"
                 R"("u":1634285425302,)"
                 R"("B":[{)"
                 R"("a":"BTC",)"
                 R"("f":"0.00004275",)"
                 R"("l":"0.00029725")"
                 R"(},{)"
                 R"("a":"LTC",)"
                 R"("f":"0.00000000",)"
                 R"("l":"0.00000000")"
                 R"(},{)"
                 R"("a":"BNB",)"
                 R"("f":"0.00041226",)"
                 R"("l":"0.00000000")"
                 R"(})"
                 R"(])"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "sj9ht0LN4uqn6kILaJpcsmZ5q2bjVInmmJPl8PdStouqzwHiwHgbwEaBm1ai"sv);
    CHECK(obj.data.event_type == json::EventType::OUTBOUND_ACCOUNT_POSITION);
    CHECK(obj.data.event_time == 1634285425303ms);
    CHECK(obj.data.time_of_last_account_update == 1634285425302ms);
    auto &balances = obj.data.balances;
    REQUIRE(std::size(balances) == 3);
    auto &balance_0 = balances[0];
    CHECK(balance_0.asset == "BTC"sv);
    CHECK(balance_0.free_amount == 0.00004275_a);
    CHECK(balance_0.locked_amount == 0.00029725_a);
    auto &balance_1 = balances[1];
    CHECK(balance_1.asset == "LTC"sv);
    CHECK(balance_1.free_amount == 0.0_a);
    CHECK(balance_1.locked_amount == 0.0_a);
    auto &balance_2 = balances[2];
    CHECK(balance_2.asset == "BNB"sv);
    CHECK(balance_2.free_amount == 0.00041226_a);
    CHECK(balance_2.locked_amount == 0.0_a);
  };
  UserStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("maker_new", "[json_outbound_account_position]") {
  auto message = R"({)"
                 R"("stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX",)"
                 R"("data":{"e":"outboundAccountPosition",)"
                 R"("E":1634906177360,)"
                 R"("u":1634906177360,)"
                 R"("B":[{)"
                 R"("a":"LTC",)"
                 R"("f":"0.00000000",)"
                 R"("l":"0.00000000")"
                 R"(},{)"
                 R"("a":"BNB",)"
                 R"("f":"0.00041226",)"
                 R"("l":"0.00000000")"
                 R"(},{)"
                 R"("a":"USDT",)"
                 R"("f":"1.31364261",)"
                 R"("l":"19.83000000")"
                 R"(})"
                 R"(])"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX"sv);
    CHECK(obj.data.event_type == json::EventType::OUTBOUND_ACCOUNT_POSITION);
    CHECK(obj.data.event_time == 1634906177360ms);
    CHECK(obj.data.time_of_last_account_update == 1634906177360ms);
    auto &balances = obj.data.balances;
    REQUIRE(std::size(balances) == 3);
    auto &balance_0 = balances[0];
    CHECK(balance_0.asset == "LTC"sv);
    CHECK(balance_0.free_amount == 0.0_a);
    CHECK(balance_0.locked_amount == 0.0_a);
    auto &balance_1 = balances[1];
    CHECK(balance_1.asset == "BNB"sv);
    CHECK(balance_1.free_amount == 0.00041226_a);
    CHECK(balance_1.locked_amount == 0.0_a);
    auto &balance_2 = balances[2];
    CHECK(balance_2.asset == "USDT"sv);
    CHECK(balance_2.free_amount == 1.31364261_a);
    CHECK(balance_2.locked_amount == 19.83_a);
  };
  UserStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("maker_filled", "[json_outbound_account_position]") {
  auto message = R"({)"
                 R"("stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX",)"
                 R"("data":{)"
                 R"("e":"outboundAccountPosition",)"
                 R"("E":1634906229934,)"
                 R"("u":1634906229933,)"
                 R"("B":[{)"
                 R"("a":"LTC",)"
                 R"("f":"0.10000000",)"
                 R"("l":"0.00000000")"
                 R"(},{)"
                 R"("a":"BNB",)"
                 R"("f":"0.00038207",)"
                 R"("l":"0.00000000")"
                 R"(},{)"
                 R"("a":"USDT",)"
                 R"("f":"1.31364261",)"
                 R"("l":"0.00000000")"
                 R"(})"
                 R"(])"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX"sv);
    CHECK(obj.data.event_type == json::EventType::OUTBOUND_ACCOUNT_POSITION);
    CHECK(obj.data.event_time == 1634906229934ms);
    CHECK(obj.data.time_of_last_account_update == 1634906229933ms);
    auto &balances = obj.data.balances;
    REQUIRE(std::size(balances) == 3);
    auto &balance_0 = balances[0];
    CHECK(balance_0.asset == "LTC"sv);
    CHECK(balance_0.free_amount == 0.1_a);
    CHECK(balance_0.locked_amount == 0.0_a);
    auto &balance_1 = balances[1];
    CHECK(balance_1.asset == "BNB"sv);
    CHECK(balance_1.free_amount == 0.00038207_a);
    CHECK(balance_1.locked_amount == 0.0_a);
    auto &balance_2 = balances[2];
    CHECK(balance_2.asset == "USDT"sv);
    CHECK(balance_2.free_amount == 1.31364261_a);
    CHECK(balance_2.locked_amount == 0.0_a);
  };
  UserStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
