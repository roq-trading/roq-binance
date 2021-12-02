/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/user_stream_parser.h"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

TEST(json_execution_report, simple) {
  auto message = R"({)"
                 R"("e":"executionReport",)"
                 R"("E":1634211568285,)"
                 R"("s":"BTCBUSD",)"
                 R"("c":"web_5440a9bd3e684127861fbc54783f70c4",)"
                 R"("S":"BUY",)"
                 R"("o":"LIMIT",)"
                 R"("f":"GTC",)"
                 R"("q":"0.00034000",)"
                 R"("p":"57883.65000000",)"
                 R"("P":"0.00000000",)"
                 R"("F":"0.00000000",)"
                 R"("g":-1,)"
                 R"("C":"",)"
                 R"("x":"NEW",)"
                 R"("X":"NEW",)"
                 R"("r":"NONE",)"
                 R"("i":3511418495,)"
                 R"("l":"0.00000000",)"
                 R"("z":"0.00000000",)"
                 R"("L":"0.00000000",)"
                 R"("n":"0",)"
                 R"("N":null,)"
                 R"("T":1634211568285,)"
                 R"("t":-1,)"
                 R"("I":7244228935,)"
                 R"("w":true,)"
                 R"("m":false,)"
                 R"("M":false,)"
                 R"("O":1634211568285,)"
                 R"("Z":"0.00000000",)"
                 R"("Y":"0.00000000",)"
                 R"("Q":"0.00000000")"
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::ExecutionReport>(message, buffer);
  EXPECT_EQ(obj.event_type, json::EventType::EXECUTION_REPORT);
  EXPECT_EQ(obj.event_time, 1634211568285ms);
  EXPECT_EQ(obj.symbol, "BTCBUSD"sv);
  EXPECT_EQ(obj.client_order_id, "web_5440a9bd3e684127861fbc54783f70c4"sv);
  EXPECT_EQ(obj.side, json::Side::BUY);
  EXPECT_EQ(obj.order_type, json::OrderType::LIMIT);
  EXPECT_EQ(obj.time_in_force, json::TimeInForce::GTC);
  EXPECT_DOUBLE_EQ(obj.quantity, 0.00034);
  EXPECT_DOUBLE_EQ(obj.price, 57883.65);
  EXPECT_DOUBLE_EQ(obj.stop_price, 0.0);
  EXPECT_DOUBLE_EQ(obj.iceberg_quantity, 0.0);
  EXPECT_EQ(obj.order_list_id, -1);
  EXPECT_EQ(obj.original_client_order_id, ""sv);
  EXPECT_EQ(obj.current_execution_type, json::ExecutionType::NEW);
  EXPECT_EQ(obj.current_order_status, json::OrderStatus::NEW);
  EXPECT_EQ(obj.order_reject_reason, "NONE"sv);
  EXPECT_EQ(obj.order_id, 3511418495);
  EXPECT_DOUBLE_EQ(obj.last_executed_quantity, 0.0);
  EXPECT_DOUBLE_EQ(obj.cumulative_filled_quantity, 0.0);
  EXPECT_DOUBLE_EQ(obj.last_executed_price, 0.0);
  EXPECT_DOUBLE_EQ(obj.commission_amount, 0.0);
  EXPECT_EQ(obj.commission_asset, ""sv);
  EXPECT_EQ(obj.transaction_time, 1634211568285ms);
  EXPECT_EQ(obj.trade_id, -1);
  EXPECT_EQ(obj.order_on_book, true);
  EXPECT_EQ(obj.is_trade_maker, false);
  EXPECT_EQ(obj.order_creation_time, 1634211568285ms);
  EXPECT_DOUBLE_EQ(obj.cumulative_quote_asset_transacted_quantity, 0.0);
  EXPECT_DOUBLE_EQ(obj.last_quote_asset_transacted_quantity, 0.0);
  EXPECT_DOUBLE_EQ(obj.quote_order_qty, 0.0);
}

TEST(json_execution_report, canceled) {
  auto message = R"({)"
                 R"("e":"executionReport",)"
                 R"("E":1634215338588,)"
                 R"("s":"LTCBTC",)"
                 R"("c":"web_24d4e429eb4f44d0ad03ab240c909ac2",)"
                 R"("S":"BUY",)"
                 R"("o":"LIMIT",)"
                 R"("f":"GTC",)"
                 R"("q":"0.10000000",)"
                 R"("p":"0.00304100",)"
                 R"("P":"0.00000000",)"
                 R"("F":"0.00000000",)"
                 R"("g":-1,)"
                 R"("C":"qQAC6gMAAQAAS-jxw4MW",)"
                 R"("x":"CANCELED",)"
                 R"("X":"CANCELED",)"
                 R"("r":"NONE",)"
                 R"("i":778507063,)"
                 R"("l":"0.00000000",)"
                 R"("z":"0.00000000",)"
                 R"("L":"0.00000000",)"
                 R"("n":"0",)"
                 R"("N":null,)"
                 R"("T":1634215338587,)"
                 R"("t":-1,)"
                 R"("I":1618956101,)"
                 R"("w":false,)"
                 R"("m":false,)"
                 R"("M":false,)"
                 R"("O":1634214384058,)"
                 R"("Z":"0.00000000",)"
                 R"("Y":"0.00000000",)"
                 R"("Q":"0.00000000")"
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::ExecutionReport>(message, buffer);
  EXPECT_EQ(obj.event_type, json::EventType::EXECUTION_REPORT);
  EXPECT_EQ(obj.event_time, 1634215338588ms);
  EXPECT_EQ(obj.symbol, "LTCBTC"sv);
  EXPECT_EQ(obj.client_order_id, "web_24d4e429eb4f44d0ad03ab240c909ac2"sv);
  EXPECT_EQ(obj.side, json::Side::BUY);
  EXPECT_EQ(obj.order_type, json::OrderType::LIMIT);
  EXPECT_EQ(obj.time_in_force, json::TimeInForce::GTC);
  EXPECT_DOUBLE_EQ(obj.quantity, 0.1);
  EXPECT_DOUBLE_EQ(obj.price, 0.003041);
  EXPECT_DOUBLE_EQ(obj.stop_price, 0.0);
  EXPECT_DOUBLE_EQ(obj.iceberg_quantity, 0.0);
  EXPECT_EQ(obj.order_list_id, -1);
  EXPECT_EQ(obj.original_client_order_id, "qQAC6gMAAQAAS-jxw4MW"sv);
  EXPECT_EQ(obj.current_execution_type, json::ExecutionType::CANCELED);
  EXPECT_EQ(obj.current_order_status, json::OrderStatus::CANCELED);
  EXPECT_EQ(obj.order_reject_reason, "NONE"sv);
  EXPECT_EQ(obj.order_id, 778507063);
  EXPECT_DOUBLE_EQ(obj.last_executed_quantity, 0.0);
  EXPECT_DOUBLE_EQ(obj.cumulative_filled_quantity, 0.0);
  EXPECT_DOUBLE_EQ(obj.last_executed_price, 0.0);
  EXPECT_DOUBLE_EQ(obj.commission_amount, 0.0);
  EXPECT_EQ(obj.commission_asset, ""sv);
  EXPECT_EQ(obj.transaction_time, 1634215338587ms);
  EXPECT_EQ(obj.trade_id, -1);
  EXPECT_EQ(obj.order_on_book, false);
  EXPECT_EQ(obj.is_trade_maker, false);
  EXPECT_EQ(obj.order_creation_time, 1634214384058ms);
  EXPECT_DOUBLE_EQ(obj.cumulative_quote_asset_transacted_quantity, 0.0);
  EXPECT_DOUBLE_EQ(obj.last_quote_asset_transacted_quantity, 0.0);
  EXPECT_DOUBLE_EQ(obj.quote_order_qty, 0.0);
}

TEST(json_execution_report, stream) {
  auto message = R"({)"
                 R"("stream":"UiujlmpQLUNOGNRDGzbC4NNza0fdtGtTFwrZPWwCR97UeUor2gZrpgvl4mz1",)"
                 R"("data":{)"
                 R"("e":"executionReport",)"
                 R"("E":1634211568285,)"
                 R"("s":"BTCBUSD",)"
                 R"("c":"web_5440a9bd3e684127861fbc54783f70c4",)"
                 R"("S":"BUY",)"
                 R"("o":"LIMIT",)"
                 R"("f":"GTC",)"
                 R"("q":"0.00034000",)"
                 R"("p":"57883.65000000",)"
                 R"("P":"0.00000000",)"
                 R"("F":"0.00000000",)"
                 R"("g":-1,)"
                 R"("C":"",)"
                 R"("x":"NEW",)"
                 R"("X":"NEW",)"
                 R"("r":"NONE",)"
                 R"("i":3511418495,)"
                 R"("l":"0.00000000",)"
                 R"("z":"0.00000000",)"
                 R"("L":"0.00000000",)"
                 R"("n":"0",)"
                 R"("N":null,)"
                 R"("T":1634211568285,)"
                 R"("t":-1,)"
                 R"("I":7244228935,)"
                 R"("w":true,)"
                 R"("m":false,)"
                 R"("M":false,)"
                 R"("O":1634211568285,)"
                 R"("Z":"0.00000000",)"
                 R"("Y":"0.00000000",)"
                 R"("Q":"0.00000000")"
                 R"(})"
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto trace_info = server::create_trace_info();
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(const server::Trace<json::OutboundAccountPosition> &) override { FAIL(); }
    void operator()(const server::Trace<json::BalanceUpdate> &) override { FAIL(); }
    void operator()(const server::Trace<json::ExecutionReport> &) override { found_ = true; }
    void operator()(const server::Trace<json::ListStatus> &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  EXPECT_TRUE(static_cast<bool>(handler));
}

TEST(json_execution_report, stream_maker_new) {
  auto message = R"({)"
                 R"("stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX",)"
                 R"("data":{)"
                 R"("e":"executionReport",)"
                 R"("E":1634906177360,)"
                 R"("s":"LTCUSDT",)"
                 R"("c":"SwAC6wMAAQAA8foJ1iQX",)"
                 R"("S":"BUY",)"
                 R"("o":"LIMIT",)"
                 R"("f":"GTC",)"
                 R"("q":"0.10000000",)"
                 R"("p":"198.30000000",)"
                 R"("P":"0.00000000",)"
                 R"("F":"0.00000000",)"
                 R"("g":-1,)"
                 R"("C":"",)"
                 R"("x":"NEW",)"
                 R"("X":"NEW",)"
                 R"("r":"NONE",)"
                 R"("i":2426862755,)"
                 R"("l":"0.00000000",)"
                 R"("z":"0.00000000",)"
                 R"("L":"0.00000000",)"
                 R"("n":"0",)"
                 R"("N":null,)"
                 R"("T":1634906177360,)"
                 R"("t":-1,)"
                 R"("I":5048624959,)"
                 R"("w":true,)"
                 R"("m":false,)"
                 R"("M":false,)"
                 R"("O":1634906177360,)"
                 R"("Z":"0.00000000",)"
                 R"("Y":"0.00000000",)"
                 R"("Q":"0.00000000")"
                 R"(})"
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto trace_info = server::create_trace_info();
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(const server::Trace<json::OutboundAccountPosition> &) override { FAIL(); }
    void operator()(const server::Trace<json::BalanceUpdate> &) override { FAIL(); }
    void operator()(const server::Trace<json::ExecutionReport> &event) override {
      found_ = true;
      auto &[_, obj] = event;
      EXPECT_EQ(obj.event_type, json::EventType::EXECUTION_REPORT);
      EXPECT_EQ(obj.event_time, 1634906177360ms);
      EXPECT_EQ(obj.symbol, "LTCUSDT"sv);
      EXPECT_EQ(obj.client_order_id, "SwAC6wMAAQAA8foJ1iQX"sv);
      EXPECT_EQ(obj.side, json::Side::BUY);
      EXPECT_EQ(obj.order_type, json::OrderType::LIMIT);
      EXPECT_EQ(obj.time_in_force, json::TimeInForce::GTC);
      EXPECT_DOUBLE_EQ(obj.quantity, 0.1);
      EXPECT_DOUBLE_EQ(obj.price, 198.3);
      EXPECT_DOUBLE_EQ(obj.stop_price, 0.0);
      EXPECT_DOUBLE_EQ(obj.iceberg_quantity, 0.0);
      EXPECT_EQ(obj.order_list_id, -1);
      EXPECT_EQ(obj.original_client_order_id, ""sv);
      EXPECT_EQ(obj.current_execution_type, json::ExecutionType::NEW);
      EXPECT_EQ(obj.current_order_status, json::OrderStatus::NEW);
      EXPECT_EQ(obj.order_reject_reason, "NONE"sv);
      EXPECT_EQ(obj.order_id, 2426862755);
      EXPECT_DOUBLE_EQ(obj.last_executed_quantity, 0.0);
      EXPECT_DOUBLE_EQ(obj.cumulative_filled_quantity, 0.0);
      EXPECT_DOUBLE_EQ(obj.last_executed_price, 0.0);
      EXPECT_DOUBLE_EQ(obj.commission_amount, 0.0);
      EXPECT_EQ(obj.commission_asset, ""sv);
      EXPECT_EQ(obj.transaction_time, 1634906177360ms);
      EXPECT_EQ(obj.trade_id, -1);
      EXPECT_EQ(obj.order_on_book, true);
      EXPECT_EQ(obj.is_trade_maker, false);
      EXPECT_EQ(obj.order_creation_time, 1634906177360ms);
      EXPECT_DOUBLE_EQ(obj.cumulative_quote_asset_transacted_quantity, 0.0);
      EXPECT_DOUBLE_EQ(obj.last_quote_asset_transacted_quantity, 0.0);
      EXPECT_DOUBLE_EQ(obj.quote_order_qty, 0.0);
    }
    void operator()(const server::Trace<json::ListStatus> &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  EXPECT_TRUE(static_cast<bool>(handler));
}

TEST(json_execution_report, stream_maker_filled) {
  auto message = R"({)"
                 R"("stream":"x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX",)"
                 R"("data":{)"
                 R"("e":"executionReport",)"
                 R"("E":1634906229934,)"
                 R"("s":"LTCUSDT",)"
                 R"("c":"SwAC6wMAAQAA8foJ1iQX",)"
                 R"("S":"BUY",)"
                 R"("o":"LIMIT",)"
                 R"("f":"GTC",)"
                 R"("q":"0.10000000",)"
                 R"("p":"198.30000000",)"
                 R"("P":"0.00000000",)"
                 R"("F":"0.00000000",)"
                 R"("g":-1,)"
                 R"("C":"",)"
                 R"("x":"TRADE",)"
                 R"("X":"FILLED",)"
                 R"("r":"NONE",)"
                 R"("i":2426862755,)"
                 R"("l":"0.10000000",)"
                 R"("z":"0.10000000",)"
                 R"("L":"198.30000000",)"
                 R"("n":"0.00003019",)"
                 R"("N":"BNB",)"
                 R"("T":1634906229933,)"
                 R"("t":207081035,)"
                 R"("I":5048627145,)"
                 R"("w":false,)"
                 R"("m":true,)"
                 R"("M":true,)"
                 R"("O":1634906177360,)"
                 R"("Z":"19.83000000",)"
                 R"("Y":"19.83000000",)"
                 R"("Q":"0.00000000")"
                 R"(})"
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto trace_info = server::create_trace_info();
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(const server::Trace<json::OutboundAccountPosition> &) override { FAIL(); }
    void operator()(const server::Trace<json::BalanceUpdate> &) override { FAIL(); }
    void operator()(const server::Trace<json::ExecutionReport> &event) override {
      found_ = true;
      auto &[_, obj] = event;
      EXPECT_EQ(obj.event_type, json::EventType::EXECUTION_REPORT);
      EXPECT_EQ(obj.event_time, 1634906229934ms);
      EXPECT_EQ(obj.symbol, "LTCUSDT"sv);
      EXPECT_EQ(obj.client_order_id, "SwAC6wMAAQAA8foJ1iQX"sv);
      EXPECT_EQ(obj.side, json::Side::BUY);
      EXPECT_EQ(obj.order_type, json::OrderType::LIMIT);
      EXPECT_EQ(obj.time_in_force, json::TimeInForce::GTC);
      EXPECT_DOUBLE_EQ(obj.quantity, 0.1);
      EXPECT_DOUBLE_EQ(obj.price, 198.3);
      EXPECT_DOUBLE_EQ(obj.stop_price, 0.0);
      EXPECT_DOUBLE_EQ(obj.iceberg_quantity, 0.0);
      EXPECT_EQ(obj.order_list_id, -1);
      EXPECT_EQ(obj.original_client_order_id, ""sv);
      EXPECT_EQ(obj.current_execution_type, json::ExecutionType::TRADE);
      EXPECT_EQ(obj.current_order_status, json::OrderStatus::FILLED);
      EXPECT_EQ(obj.order_reject_reason, "NONE"sv);
      EXPECT_EQ(obj.order_id, 2426862755);
      EXPECT_DOUBLE_EQ(obj.last_executed_quantity, 0.1);
      EXPECT_DOUBLE_EQ(obj.cumulative_filled_quantity, 0.1);
      EXPECT_DOUBLE_EQ(obj.last_executed_price, 198.3);
      EXPECT_DOUBLE_EQ(obj.commission_amount, 0.00003019);
      EXPECT_EQ(obj.commission_asset, "BNB"sv);
      EXPECT_EQ(obj.transaction_time, 1634906229933ms);
      EXPECT_EQ(obj.trade_id, 207081035);
      EXPECT_EQ(obj.order_on_book, false);
      EXPECT_EQ(obj.is_trade_maker, true);
      EXPECT_EQ(obj.order_creation_time, 1634906177360ms);
      EXPECT_DOUBLE_EQ(obj.cumulative_quote_asset_transacted_quantity, 19.83);
      EXPECT_DOUBLE_EQ(obj.last_quote_asset_transacted_quantity, 19.83);
      EXPECT_DOUBLE_EQ(obj.quote_order_qty, 0.0);
    }
    void operator()(const server::Trace<json::ListStatus> &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info);
  EXPECT_TRUE(static_cast<bool>(handler));
}
