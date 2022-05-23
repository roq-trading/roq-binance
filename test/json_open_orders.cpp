/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/open_orders.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_open_orders_simple_empty", "[json_open_orders]") {
  auto message = R"([])";
  core::Buffer buffer_(8192);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::OpenOrders>(message, buffer);
  REQUIRE(std::size(obj.data) == 0);
}

TEST_CASE("json_open_orders_simple", "[json_open_orders]") {
  auto message = R"([{)"
                 R"("symbol":"LTCBTC",)"
                 R"("orderId":778507063,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"qQAC6gMAAQAAS-jxw4MW",)"
                 R"("price":"0.00304100",)"
                 R"("origQty":"0.10000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"NEW",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("stopPrice":"0.00000000",)"
                 R"("icebergQty":"0.00000000",)"
                 R"("time":1634214384058,)"
                 R"("updateTime":1634214384058,)"
                 R"("isWorking":true,)"
                 R"("origQuoteOrderQty":"0.00000000")"
                 R"(})"
                 R"(])";
  core::Buffer buffer_(8192);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::OpenOrders>(message, buffer);
  auto &data = obj.data;
  REQUIRE(std::size(data) == 1);
  auto &d0 = data[0];
  CHECK(d0.symbol == "LTCBTC"sv);
  CHECK(d0.order_id == 778507063);
  CHECK(d0.order_list_id == -1);
  CHECK(d0.client_order_id == "qQAC6gMAAQAAS-jxw4MW"sv);
  CHECK(d0.price == 0.003041_a);
  CHECK(d0.orig_qty == 0.1_a);
  CHECK(d0.executed_qty == 0.0_a);
  CHECK(d0.cummulative_quote_qty == 0.0_a);
  CHECK(d0.status == json::OrderStatus::NEW);
  CHECK(d0.time_in_force == json::TimeInForce::GTC);
  CHECK(d0.type == json::OrderType::LIMIT);
  CHECK(d0.side == json::Side::BUY);
  CHECK(d0.stop_price == 0.0_a);
  CHECK(d0.iceberg_qty == 0.0_a);
  CHECK(d0.time == 1634214384058ms);
  CHECK(d0.update_time == 1634214384058ms);
  CHECK(d0.is_working == true);
  CHECK(d0.orig_quote_order_qty == 0.0_a);
}
