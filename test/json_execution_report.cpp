/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"
#include "roq/core/json/parser.hpp"

#include "roq/binance/json/user_stream_parser.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_execution_report_simple", "[json_execution_report]") {
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
  core::json::BufferStack buffer{8192, 1};
  json::ExecutionReport obj{message, buffer};
  CHECK(obj.event_type == json::EventType::EXECUTION_REPORT);
  CHECK(obj.event_time == 1634211568285ms);
  CHECK(obj.symbol == "BTCBUSD"sv);
  CHECK(obj.client_order_id == "web_5440a9bd3e684127861fbc54783f70c4"sv);
  CHECK(obj.side == json::Side::BUY);
  CHECK(obj.order_type == json::OrderType::LIMIT);
  CHECK(obj.time_in_force == json::TimeInForce::GTC);
  CHECK(obj.quantity == 0.00034_a);
  CHECK(obj.price == 57883.65_a);
  CHECK(obj.stop_price == 0.0_a);
  CHECK(obj.iceberg_quantity == 0.0_a);
  CHECK(obj.order_list_id == -1);
  CHECK(std::empty(obj.original_client_order_id));
  CHECK(obj.current_execution_type == json::ExecutionType::NEW);
  CHECK(obj.current_order_status == json::OrderStatus::NEW);
  CHECK(obj.order_reject_reason == "NONE"sv);
  CHECK(obj.order_id == 3511418495);
  CHECK(obj.last_executed_quantity == 0.0_a);
  CHECK(obj.cumulative_filled_quantity == 0.0_a);
  CHECK(obj.last_executed_price == 0.0_a);
  CHECK(obj.commission_amount == 0.0_a);
  CHECK(std::empty(obj.commission_asset));
  CHECK(obj.transaction_time == 1634211568285ms);
  CHECK(obj.trade_id == -1);
  CHECK(obj.order_on_book == true);
  CHECK(obj.is_trade_maker == false);
  CHECK(obj.order_creation_time == 1634211568285ms);
  CHECK(obj.cumulative_quote_asset_transacted_quantity == 0.0_a);
  CHECK(obj.last_quote_asset_transacted_quantity == 0.0_a);
  CHECK(obj.quote_order_qty == 0.0_a);
}

TEST_CASE("json_execution_report_canceled", "[json_execution_report]") {
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
  core::json::BufferStack buffer{8192, 1};
  json::ExecutionReport obj{message, buffer};
  CHECK(obj.event_type == json::EventType::EXECUTION_REPORT);
  CHECK(obj.event_time == 1634215338588ms);
  CHECK(obj.symbol == "LTCBTC"sv);
  CHECK(obj.client_order_id == "web_24d4e429eb4f44d0ad03ab240c909ac2"sv);
  CHECK(obj.side == json::Side::BUY);
  CHECK(obj.order_type == json::OrderType::LIMIT);
  CHECK(obj.time_in_force == json::TimeInForce::GTC);
  CHECK(obj.quantity == 0.1_a);
  CHECK(obj.price == 0.003041_a);
  CHECK(obj.stop_price == 0.0_a);
  CHECK(obj.iceberg_quantity == 0.0_a);
  CHECK(obj.order_list_id == -1);
  CHECK(obj.original_client_order_id == "qQAC6gMAAQAAS-jxw4MW"sv);
  CHECK(obj.current_execution_type == json::ExecutionType::CANCELED);
  CHECK(obj.current_order_status == json::OrderStatus::CANCELED);
  CHECK(obj.order_reject_reason == "NONE"sv);
  CHECK(obj.order_id == 778507063);
  CHECK(obj.last_executed_quantity == 0.0_a);
  CHECK(obj.cumulative_filled_quantity == 0.0_a);
  CHECK(obj.last_executed_price == 0.0_a);
  CHECK(obj.commission_amount == 0.0_a);
  CHECK(std::empty(obj.commission_asset));
  CHECK(obj.transaction_time == 1634215338587ms);
  CHECK(obj.trade_id == -1);
  CHECK(obj.order_on_book == false);
  CHECK(obj.is_trade_maker == false);
  CHECK(obj.order_creation_time == 1634214384058ms);
  CHECK(obj.cumulative_quote_asset_transacted_quantity == 0.0_a);
  CHECK(obj.last_quote_asset_transacted_quantity == 0.0_a);
  CHECK(obj.quote_order_qty == 0.0_a);
}

TEST_CASE("json_execution_report_stream", "[json_execution_report]") {
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
  core::json::BufferStack buffer{8192, 1};
  TraceInfo trace_info;
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(Trace<json::OutboundAccountPosition> const &) override { FAIL(); }
    void operator()(Trace<json::BalanceUpdate> const &) override { FAIL(); }
    void operator()(Trace<json::ExecutionReport> const &) override { found_ = true; }
    void operator()(Trace<json::ListStatus> const &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info, false);
  CHECK(static_cast<bool>(handler) == true);
}

TEST_CASE("json_execution_report_stream_maker_new", "[json_execution_report]") {
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
  core::json::BufferStack buffer{8192, 1};
  TraceInfo trace_info;
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(Trace<json::OutboundAccountPosition> const &) override { FAIL(); }
    void operator()(Trace<json::BalanceUpdate> const &) override { FAIL(); }
    void operator()(Trace<json::ExecutionReport> const &event) override {
      found_ = true;
      auto &[_, obj] = event;
      CHECK(obj.event_type == json::EventType::EXECUTION_REPORT);
      CHECK(obj.event_time == 1634906177360ms);
      CHECK(obj.symbol == "LTCUSDT"sv);
      CHECK(obj.client_order_id == "SwAC6wMAAQAA8foJ1iQX"sv);
      CHECK(obj.side == json::Side::BUY);
      CHECK(obj.order_type == json::OrderType::LIMIT);
      CHECK(obj.time_in_force == json::TimeInForce::GTC);
      CHECK(obj.quantity == 0.1_a);
      CHECK(obj.price == 198.3_a);
      CHECK(obj.stop_price == 0.0_a);
      CHECK(obj.iceberg_quantity == 0.0_a);
      CHECK(obj.order_list_id == -1);
      CHECK(std::empty(obj.original_client_order_id));
      CHECK(obj.current_execution_type == json::ExecutionType::NEW);
      CHECK(obj.current_order_status == json::OrderStatus::NEW);
      CHECK(obj.order_reject_reason == "NONE"sv);
      CHECK(obj.order_id == 2426862755);
      CHECK(obj.last_executed_quantity == 0.0_a);
      CHECK(obj.cumulative_filled_quantity == 0.0_a);
      CHECK(obj.last_executed_price == 0.0_a);
      CHECK(obj.commission_amount == 0.0_a);
      CHECK(std::empty(obj.commission_asset));
      CHECK(obj.transaction_time == 1634906177360ms);
      CHECK(obj.trade_id == -1);
      CHECK(obj.order_on_book == true);
      CHECK(obj.is_trade_maker == false);
      CHECK(obj.order_creation_time == 1634906177360ms);
      CHECK(obj.cumulative_quote_asset_transacted_quantity == 0.0_a);
      CHECK(obj.last_quote_asset_transacted_quantity == 0.0_a);
      CHECK(obj.quote_order_qty == 0.0_a);
    }
    void operator()(Trace<json::ListStatus> const &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info, false);
  CHECK(static_cast<bool>(handler) == true);
}

TEST_CASE("json_execution_report_stream_maker_filled", "[json_execution_report]") {
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
  core::json::BufferStack buffer{8192, 1};
  TraceInfo trace_info;
  struct MyHandler final : public json::UserStreamParser::Handler {
    void operator()(Trace<json::OutboundAccountPosition> const &) override { FAIL(); }
    void operator()(Trace<json::BalanceUpdate> const &) override { FAIL(); }
    void operator()(Trace<json::ExecutionReport> const &event) override {
      found_ = true;
      auto &[_, obj] = event;
      CHECK(obj.event_type == json::EventType::EXECUTION_REPORT);
      CHECK(obj.event_time == 1634906229934ms);
      CHECK(obj.symbol == "LTCUSDT"sv);
      CHECK(obj.client_order_id == "SwAC6wMAAQAA8foJ1iQX"sv);
      CHECK(obj.side == json::Side::BUY);
      CHECK(obj.order_type == json::OrderType::LIMIT);
      CHECK(obj.time_in_force == json::TimeInForce::GTC);
      CHECK(obj.quantity == 0.1_a);
      CHECK(obj.price == 198.3_a);
      CHECK(obj.stop_price == 0.0_a);
      CHECK(obj.iceberg_quantity == 0.0_a);
      CHECK(obj.order_list_id == -1);
      CHECK(std::empty(obj.original_client_order_id));
      CHECK(obj.current_execution_type == json::ExecutionType::TRADE);
      CHECK(obj.current_order_status == json::OrderStatus::FILLED);
      CHECK(obj.order_reject_reason == "NONE"sv);
      CHECK(obj.order_id == 2426862755);
      CHECK(obj.last_executed_quantity == 0.1_a);
      CHECK(obj.cumulative_filled_quantity == 0.1_a);
      CHECK(obj.last_executed_price == 198.3_a);
      CHECK(obj.commission_amount == 0.00003019_a);
      CHECK(obj.commission_asset == "BNB"sv);
      CHECK(obj.transaction_time == 1634906229933ms);
      CHECK(obj.trade_id == 207081035);
      CHECK(obj.order_on_book == false);
      CHECK(obj.is_trade_maker == true);
      CHECK(obj.order_creation_time == 1634906177360ms);
      CHECK(obj.cumulative_quote_asset_transacted_quantity == 19.83_a);
      CHECK(obj.last_quote_asset_transacted_quantity == 19.83_a);
      CHECK(obj.quote_order_qty == 0.0_a);
    }
    void operator()(Trace<json::ListStatus> const &) override { FAIL(); }

    operator bool() const { return found_; }

   private:
    bool found_ = false;
  } handler;
  json::UserStreamParser::dispatch(handler, message, buffer, trace_info, false);
  CHECK(static_cast<bool>(handler) == true);
}
