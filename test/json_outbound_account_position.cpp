/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/user_stream_parser.h"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
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
  EXPECT_EQ(b0.asset, "BTC"sv);
  EXPECT_DOUBLE_EQ(b0.free_amount, 0.00004275);
  EXPECT_DOUBLE_EQ(b0.locked_amount, 0.00029725);
  auto &b1 = balances[1];
  EXPECT_EQ(b1.asset, "LTC"sv);
  EXPECT_DOUBLE_EQ(b1.free_amount, 0.0);
  EXPECT_DOUBLE_EQ(b1.locked_amount, 0.0);
  auto &b2 = balances[2];
  EXPECT_EQ(b2.asset, "BNB"sv);
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
  auto trace_info = server::create_trace_info();
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(const server::Trace<json::OutboundAccountPosition> &) override {
      found_ = true;
    }
    void operator()(const server::Trace<json::BalanceUpdate> &) override { FAIL(); }
    void operator()(const server::Trace<json::ExecutionReport> &) override { FAIL(); }
    void operator()(const server::Trace<json::ListStatus> &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  EXPECT_TRUE(static_cast<bool>(handler));
}

TEST(json_outbound_account_position, stream_maker_new) {
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
    void operator()(const server::Trace<json::OutboundAccountPosition> &event) override {
      found_ = true;
      auto &[_, obj] = event;
      EXPECT_EQ(obj.event_type, json::EventType::OUTBOUND_ACCOUNT_POSITION);
      EXPECT_EQ(obj.event_time, 1634906177360ms);
      EXPECT_EQ(obj.time_of_last_account_update, 1634906177360ms);
      auto &balances = obj.balances;
      ASSERT_EQ(std::size(balances), 3);
      auto &b0 = balances[0];
      EXPECT_EQ(b0.asset, "LTC"sv);
      EXPECT_DOUBLE_EQ(b0.free_amount, 0.0);
      EXPECT_DOUBLE_EQ(b0.locked_amount, 0.0);
      auto &b1 = balances[1];
      EXPECT_EQ(b1.asset, "BNB"sv);
      EXPECT_DOUBLE_EQ(b1.free_amount, 0.00041226);
      EXPECT_DOUBLE_EQ(b1.locked_amount, 0.0);
      auto &b2 = balances[2];
      EXPECT_EQ(b2.asset, "USDT"sv);
      EXPECT_DOUBLE_EQ(b2.free_amount, 1.31364261);
      EXPECT_DOUBLE_EQ(b2.locked_amount, 19.83);
    }
    void operator()(const server::Trace<json::BalanceUpdate> &) override { FAIL(); }
    void operator()(const server::Trace<json::ExecutionReport> &) override { FAIL(); }
    void operator()(const server::Trace<json::ListStatus> &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  EXPECT_TRUE(static_cast<bool>(handler));
}

TEST(json_outbound_account_position, stream_maker_filled) {
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
    void operator()(const server::Trace<json::OutboundAccountPosition> &event) override {
      found_ = true;
      auto &[_, obj] = event;
      EXPECT_EQ(obj.event_type, json::EventType::OUTBOUND_ACCOUNT_POSITION);
      EXPECT_EQ(obj.event_time, 1634906229934ms);
      EXPECT_EQ(obj.time_of_last_account_update, 1634906229933ms);
      auto &balances = obj.balances;
      ASSERT_EQ(std::size(balances), 3);
      auto &b0 = balances[0];
      EXPECT_EQ(b0.asset, "LTC"sv);
      EXPECT_DOUBLE_EQ(b0.free_amount, 0.1);
      EXPECT_DOUBLE_EQ(b0.locked_amount, 0.0);
      auto &b1 = balances[1];
      EXPECT_EQ(b1.asset, "BNB"sv);
      EXPECT_DOUBLE_EQ(b1.free_amount, 0.00038207);
      EXPECT_DOUBLE_EQ(b1.locked_amount, 0.0);
      auto &b2 = balances[2];
      EXPECT_EQ(b2.asset, "USDT"sv);
      EXPECT_DOUBLE_EQ(b2.free_amount, 1.31364261);
      EXPECT_DOUBLE_EQ(b2.locked_amount, 0.0);
    }
    void operator()(const server::Trace<json::BalanceUpdate> &) override { FAIL(); }
    void operator()(const server::Trace<json::ExecutionReport> &) override { FAIL(); }
    void operator()(const server::Trace<json::ListStatus> &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  EXPECT_TRUE(static_cast<bool>(handler));
}
