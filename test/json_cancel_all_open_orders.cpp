/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/cancel_all_open_orders.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_cancel_all_open_orders_simple_empty", "[json_cancel_all_open_orders]") {
  auto message = R"([])";
  core::Buffer buffer_(8192);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::CancelAllOpenOrders>(message, buffer);
  REQUIRE(std::size(obj.data) == 0);
}

TEST_CASE("json_cancel_all_open_orders_simple_1", "[json_cancel_all_open_orders]") {
  auto message = R"([{)"
                 R"( "symbol":"LTCBTC",)"
                 R"("origClientOrderId":"QAAC6gMAAQAARzE1LJQW",)"
                 R"("orderId":779222768,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"ya8dFipP337pVE3ogU1JFU",)"
                 R"("price":"0.00297800",)"
                 R"("origQty":"0.05000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY")"
                 R"(})"
                 R"(])";
  core::Buffer buffer_(8192);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::CancelAllOpenOrders>(message, buffer);
  auto &data = obj.data;
  REQUIRE(std::size(data) == 1);
  auto &d0 = data[0];
  CHECK(d0.symbol == "LTCBTC"sv);
  CHECK(d0.orig_client_order_id == "QAAC6gMAAQAARzE1LJQW"sv);
  CHECK(d0.order_id == 779222768);
  CHECK(d0.order_list_id == -1);
  CHECK(d0.client_order_id == "ya8dFipP337pVE3ogU1JFU"sv);
  CHECK(d0.price == 0.002978_a);
  CHECK(d0.orig_qty == 0.05_a);
  CHECK(d0.executed_qty == 0.0_a);
  CHECK(d0.cummulative_quote_qty == 0.0_a);
  CHECK(d0.status == json::OrderStatus::CANCELED);
  CHECK(d0.time_in_force == json::TimeInForce::GTC);
  CHECK(d0.type == json::OrderType::LIMIT);
  CHECK(d0.side == json::Side::BUY);
}

TEST_CASE("json_cancel_all_open_orders_simple_2", "[json_cancel_all_open_orders]") {
  auto message = R"([{)"
                 R"("symbol":"LTCBTC",)"
                 R"("origClientOrderId":"RAAC6wMAAQAATcQ10ZMW",)"
                 R"("orderId":779213492,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"xsyFwe7R4szBwr2N8UNBii",)"
                 R"("price":"0.00298400",)"
                 R"("origQty":"0.05000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY")"
                 R"(},{)"
                 R"("symbol":"LTCBTC",)"
                 R"("origClientOrderId":"XgAC7AMAAQAAWZxp3JMW",)"
                 R"("orderId":779214518,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"xsyFwe7R4szBwr2N8UNBii",)"
                 R"("price":"0.00298600",)"
                 R"("origQty":"0.05000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY")"
                 R"(})"
                 R"(])";
  core::Buffer buffer_(8192);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::CancelAllOpenOrders>(message, buffer);
  auto &data = obj.data;
  REQUIRE(std::size(data) == 2);
  auto &d0 = data[0];
  CHECK(d0.symbol == "LTCBTC"sv);
  CHECK(d0.orig_client_order_id == "RAAC6wMAAQAATcQ10ZMW"sv);
  CHECK(d0.order_id == 779213492);
  CHECK(d0.order_list_id == -1);
  CHECK(d0.client_order_id == "xsyFwe7R4szBwr2N8UNBii"sv);
  CHECK(d0.price == 0.002984_a);
  CHECK(d0.orig_qty == 0.05_a);
  CHECK(d0.executed_qty == 0.0_a);
  CHECK(d0.cummulative_quote_qty == 0.0_a);
  CHECK(d0.status == json::OrderStatus::CANCELED);
  CHECK(d0.time_in_force == json::TimeInForce::GTC);
  CHECK(d0.type == json::OrderType::LIMIT);
  CHECK(d0.side == json::Side::BUY);
  auto &d1 = data[1];
  CHECK(d1.symbol == "LTCBTC"sv);
  CHECK(d1.orig_client_order_id == "XgAC7AMAAQAAWZxp3JMW"sv);
  CHECK(d1.order_id == 779214518);
  CHECK(d1.order_list_id == -1);
  CHECK(d1.client_order_id == "xsyFwe7R4szBwr2N8UNBii"sv);
  CHECK(d1.price == 0.002986_a);
  CHECK(d1.orig_qty == 0.05_a);
  CHECK(d1.executed_qty == 0.0_a);
  CHECK(d1.cummulative_quote_qty == 0.0_a);
  CHECK(d1.status == json::OrderStatus::CANCELED);
  CHECK(d1.time_in_force == json::TimeInForce::GTC);
  CHECK(d1.type == json::OrderType::LIMIT);
  CHECK(d1.side == json::Side::BUY);
}
