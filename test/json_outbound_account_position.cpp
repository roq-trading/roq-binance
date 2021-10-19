/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/user_stream_parser.h"

using namespace roq;
using namespace roq::binance;

using namespace std::chrono_literals;

TEST(json_outbound_account_position, simple) {
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
  EXPECT_EQ(obj.event_type, json::EventType::OUTBOUND_ACCOUNT_POSITION);
  EXPECT_EQ(obj.event_time, 1634285425303ms);
  EXPECT_EQ(obj.time_of_last_account_update, 1634285425302ms);
  auto &balances = obj.balances;
  ASSERT_EQ(std::size(balances), 3);
  auto &b0 = balances[0];
  EXPECT_EQ(b0.asset, "BTC"_sv);
  EXPECT_DOUBLE_EQ(b0.free_amount, 0.00004275);
  EXPECT_DOUBLE_EQ(b0.locked_amount, 0.00029725);
  auto &b1 = balances[1];
  EXPECT_EQ(b1.asset, "LTC"_sv);
  EXPECT_DOUBLE_EQ(b1.free_amount, 0.0);
  EXPECT_DOUBLE_EQ(b1.locked_amount, 0.0);
  auto &b2 = balances[2];
  EXPECT_EQ(b2.asset, "BNB"_sv);
  EXPECT_DOUBLE_EQ(b2.free_amount, 0.00041226);
  EXPECT_DOUBLE_EQ(b2.locked_amount, 0.0);
}

TEST(json_outbound_account_position, stream) {
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
  server::TraceInfo trace_info;
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(const server::Trace<json::OutboundAccountInfo> &) override { FAIL(); }
    void operator()(const server::Trace<json::OutboundAccountPosition> &) override {
      found_ = true;
    }
    void operator()(const server::Trace<json::BalanceUpdate> &) override { FAIL(); }
    void operator()(const server::Trace<json::ExecutionReport> &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  EXPECT_TRUE(static_cast<bool>(handler));
}
