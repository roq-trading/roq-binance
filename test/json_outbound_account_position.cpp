/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/user_stream_parser.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_outbound_account_position_simple", "[json_outbound_account_position]") {
  auto message = R"({)"
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
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::OutboundAccountPosition>(message, buffer);
  CHECK(obj.event_type == json::EventType::OUTBOUND_ACCOUNT_POSITION);
  CHECK(obj.event_time == 1634285425303ms);
  CHECK(obj.time_of_last_account_update == 1634285425302ms);
  auto &balances = obj.balances;
  REQUIRE(std::size(balances) == 3);
  auto &b0 = balances[0];
  CHECK(b0.asset == "BTC"sv);
  CHECK(b0.free_amount == 0.00004275_a);
  CHECK(b0.locked_amount == 0.00029725_a);
  auto &b1 = balances[1];
  CHECK(b1.asset == "LTC"sv);
  CHECK(b1.free_amount == 0.0_a);
  CHECK(b1.locked_amount == 0.0_a);
  auto &b2 = balances[2];
  CHECK(b2.asset == "BNB"sv);
  CHECK(b2.free_amount == 0.00041226_a);
  CHECK(b2.locked_amount == 0.0_a);
}

TEST_CASE("json_outbound_account_position_stream", "[json_outbound_account_position]") {
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
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto trace_info = server::create_trace_info();
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(Trace<json::OutboundAccountPosition const> const &) override { found_ = true; }
    void operator()(Trace<json::BalanceUpdate const> const &) override { FAIL(); }
    void operator()(Trace<json::ExecutionReport const> const &) override { FAIL(); }
    void operator()(Trace<json::ListStatus const> const &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  CHECK(static_cast<bool>(handler) == true);
}

TEST_CASE("json_outbound_account_position_stream_maker_new", "[json_outbound_account_position]") {
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
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto trace_info = server::create_trace_info();
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(Trace<json::OutboundAccountPosition const> const &event) override {
      found_ = true;
      auto &[_, obj] = event;
      CHECK(obj.event_type == json::EventType::OUTBOUND_ACCOUNT_POSITION);
      CHECK(obj.event_time == 1634906177360ms);
      CHECK(obj.time_of_last_account_update == 1634906177360ms);
      auto &balances = obj.balances;
      REQUIRE(std::size(balances) == 3);
      auto &b0 = balances[0];
      CHECK(b0.asset == "LTC"sv);
      CHECK(b0.free_amount == 0.0_a);
      CHECK(b0.locked_amount == 0.0_a);
      auto &b1 = balances[1];
      CHECK(b1.asset == "BNB"sv);
      CHECK(b1.free_amount == 0.00041226_a);
      CHECK(b1.locked_amount == 0.0_a);
      auto &b2 = balances[2];
      CHECK(b2.asset == "USDT"sv);
      CHECK(b2.free_amount == 1.31364261_a);
      CHECK(b2.locked_amount == 19.83_a);
    }
    void operator()(Trace<json::BalanceUpdate const> const &) override { FAIL(); }
    void operator()(Trace<json::ExecutionReport const> const &) override { FAIL(); }
    void operator()(Trace<json::ListStatus const> const &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  CHECK(static_cast<bool>(handler) == true);
}

TEST_CASE("json_outbound_account_position_stream_maker_filled", "[json_outbound_account_position]") {
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
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto trace_info = server::create_trace_info();
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(Trace<json::OutboundAccountPosition const> const &event) override {
      found_ = true;
      auto &[_, obj] = event;
      CHECK(obj.event_type == json::EventType::OUTBOUND_ACCOUNT_POSITION);
      CHECK(obj.event_time == 1634906229934ms);
      CHECK(obj.time_of_last_account_update == 1634906229933ms);
      auto &balances = obj.balances;
      REQUIRE(std::size(balances) == 3);
      auto &b0 = balances[0];
      CHECK(b0.asset == "LTC"sv);
      CHECK(b0.free_amount == 0.1_a);
      CHECK(b0.locked_amount == 0.0_a);
      auto &b1 = balances[1];
      CHECK(b1.asset == "BNB"sv);
      CHECK(b1.free_amount == 0.00038207_a);
      CHECK(b1.locked_amount == 0.0_a);
      auto &b2 = balances[2];
      CHECK(b2.asset == "USDT"sv);
      CHECK(b2.free_amount == 1.31364261_a);
      CHECK(b2.locked_amount == 0.0_a);
    }
    void operator()(Trace<json::BalanceUpdate const> const &) override { FAIL(); }
    void operator()(Trace<json::ExecutionReport const> const &) override { FAIL(); }
    void operator()(Trace<json::ListStatus const> const &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  CHECK(static_cast<bool>(handler) == true);
}
