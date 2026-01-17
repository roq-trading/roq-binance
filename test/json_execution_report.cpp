/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "user_stream_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::ExecutionReport;
/*
TEST_CASE("simple", "[json_execution_report]") {
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
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
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
  };
  UserStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
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
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
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
  };
  UserStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
*/
TEST_CASE("simple", "[json_execution_report]") {
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
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "UiujlmpQLUNOGNRDGzbC4NNza0fdtGtTFwrZPWwCR97UeUor2gZrpgvl4mz1"sv);
    CHECK(obj.data.event_type == json::EventType::EXECUTION_REPORT);
    CHECK(obj.data.event_time == 1634211568285ms);
  };
  UserStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("maker_new", "[json_execution_report]") {
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
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX"sv);
    CHECK(obj.data.event_type == json::EventType::EXECUTION_REPORT);
    CHECK(obj.data.event_time == 1634906177360ms);
    CHECK(obj.data.symbol == "LTCUSDT"sv);
    CHECK(obj.data.client_order_id == "SwAC6wMAAQAA8foJ1iQX"sv);
    CHECK(obj.data.side == json::Side::BUY);
    CHECK(obj.data.order_type == json::OrderType::LIMIT);
    CHECK(obj.data.time_in_force == json::TimeInForce::GTC);
    CHECK(obj.data.quantity == 0.1_a);
    CHECK(obj.data.price == 198.3_a);
    CHECK(obj.data.stop_price == 0.0_a);
    CHECK(obj.data.iceberg_quantity == 0.0_a);
    CHECK(obj.data.order_list_id == -1);
    CHECK(std::empty(obj.data.original_client_order_id));
    CHECK(obj.data.current_execution_type == json::ExecutionType::NEW);
    CHECK(obj.data.current_order_status == json::OrderStatus::NEW);
    CHECK(obj.data.order_reject_reason == "NONE"sv);
    CHECK(obj.data.order_id == 2426862755);
    CHECK(obj.data.last_executed_quantity == 0.0_a);
    CHECK(obj.data.cumulative_filled_quantity == 0.0_a);
    CHECK(obj.data.last_executed_price == 0.0_a);
    CHECK(obj.data.commission_amount == 0.0_a);
    CHECK(std::empty(obj.data.commission_asset));
    CHECK(obj.data.transaction_time == 1634906177360ms);
    CHECK(obj.data.trade_id == -1);
    CHECK(obj.data.order_on_book == true);
    CHECK(obj.data.is_trade_maker == false);
    CHECK(obj.data.order_creation_time == 1634906177360ms);
    CHECK(obj.data.cumulative_quote_asset_transacted_quantity == 0.0_a);
    CHECK(obj.data.last_quote_asset_transacted_quantity == 0.0_a);
    CHECK(obj.data.quote_order_qty == 0.0_a);
  };
  UserStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("maker_filled", "[json_execution_report]") {
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
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "x4PghblTRhWAXEO9E0wrDhwIZ0kRXDp3I32Vg9B60nxqGNjiG1lknGi1omdX"sv);
    CHECK(obj.data.event_type == json::EventType::EXECUTION_REPORT);
    CHECK(obj.data.event_time == 1634906229934ms);
    CHECK(obj.data.symbol == "LTCUSDT"sv);
    CHECK(obj.data.client_order_id == "SwAC6wMAAQAA8foJ1iQX"sv);
    CHECK(obj.data.side == json::Side::BUY);
    CHECK(obj.data.order_type == json::OrderType::LIMIT);
    CHECK(obj.data.time_in_force == json::TimeInForce::GTC);
    CHECK(obj.data.quantity == 0.1_a);
    CHECK(obj.data.price == 198.3_a);
    CHECK(obj.data.stop_price == 0.0_a);
    CHECK(obj.data.iceberg_quantity == 0.0_a);
    CHECK(obj.data.order_list_id == -1);
    CHECK(std::empty(obj.data.original_client_order_id));
    CHECK(obj.data.current_execution_type == json::ExecutionType::TRADE);
    CHECK(obj.data.current_order_status == json::OrderStatus::FILLED);
    CHECK(obj.data.order_reject_reason == "NONE"sv);
    CHECK(obj.data.order_id == 2426862755);
    CHECK(obj.data.last_executed_quantity == 0.1_a);
    CHECK(obj.data.cumulative_filled_quantity == 0.1_a);
    CHECK(obj.data.last_executed_price == 198.3_a);
    CHECK(obj.data.commission_amount == 0.00003019_a);
    CHECK(obj.data.commission_asset == "BNB"sv);
    CHECK(obj.data.transaction_time == 1634906229933ms);
    CHECK(obj.data.trade_id == 207081035);
    CHECK(obj.data.order_on_book == false);
    CHECK(obj.data.is_trade_maker == true);
    CHECK(obj.data.order_creation_time == 1634906177360ms);
    CHECK(obj.data.cumulative_quote_asset_transacted_quantity == 19.83_a);
    CHECK(obj.data.last_quote_asset_transacted_quantity == 19.83_a);
    CHECK(obj.data.quote_order_qty == 0.0_a);
  };
  UserStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
