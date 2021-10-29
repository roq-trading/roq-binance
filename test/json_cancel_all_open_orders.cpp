/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/cancel_all_open_orders.h"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

TEST(json_cancel_all_open_orders, simple_empty) {
  auto message = R"([])";
  core::Buffer buffer_(8192);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::CancelAllOpenOrders>(message, buffer);
  ASSERT_EQ(std::size(obj.data), 0);
}

TEST(json_cancel_all_open_orders, simple_1) {
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
  ASSERT_EQ(std::size(data), 1);
  auto &d0 = data[0];
  EXPECT_EQ(d0.symbol, "LTCBTC"sv);
  EXPECT_EQ(d0.orig_client_order_id, "QAAC6gMAAQAARzE1LJQW"sv);
  EXPECT_EQ(d0.order_id, 779222768);
  EXPECT_EQ(d0.order_list_id, -1);
  EXPECT_EQ(d0.client_order_id, "ya8dFipP337pVE3ogU1JFU"sv);
  EXPECT_DOUBLE_EQ(d0.price, 0.002978);
  EXPECT_DOUBLE_EQ(d0.orig_qty, 0.05);
  EXPECT_DOUBLE_EQ(d0.executed_qty, 0.0);
  EXPECT_DOUBLE_EQ(d0.cummulative_quote_qty, 0.0);
  EXPECT_EQ(d0.status, json::OrderStatus::CANCELED);
  EXPECT_EQ(d0.time_in_force, json::TimeInForce::GTC);
  EXPECT_EQ(d0.type, json::OrderType::LIMIT);
  EXPECT_EQ(d0.side, json::Side::BUY);
}

TEST(json_cancel_all_open_orders, simple_2) {
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
  ASSERT_EQ(std::size(data), 2);
  auto &d0 = data[0];
  EXPECT_EQ(d0.symbol, "LTCBTC"sv);
  EXPECT_EQ(d0.orig_client_order_id, "RAAC6wMAAQAATcQ10ZMW"sv);
  EXPECT_EQ(d0.order_id, 779213492);
  EXPECT_EQ(d0.order_list_id, -1);
  EXPECT_EQ(d0.client_order_id, "xsyFwe7R4szBwr2N8UNBii"sv);
  EXPECT_DOUBLE_EQ(d0.price, 0.002984);
  EXPECT_DOUBLE_EQ(d0.orig_qty, 0.05);
  EXPECT_DOUBLE_EQ(d0.executed_qty, 0.0);
  EXPECT_DOUBLE_EQ(d0.cummulative_quote_qty, 0.0);
  EXPECT_EQ(d0.status, json::OrderStatus::CANCELED);
  EXPECT_EQ(d0.time_in_force, json::TimeInForce::GTC);
  EXPECT_EQ(d0.type, json::OrderType::LIMIT);
  EXPECT_EQ(d0.side, json::Side::BUY);
  auto &d1 = data[1];
  EXPECT_EQ(d1.symbol, "LTCBTC"sv);
  EXPECT_EQ(d1.orig_client_order_id, "XgAC7AMAAQAAWZxp3JMW"sv);
  EXPECT_EQ(d1.order_id, 779214518);
  EXPECT_EQ(d1.order_list_id, -1);
  EXPECT_EQ(d1.client_order_id, "xsyFwe7R4szBwr2N8UNBii"sv);
  EXPECT_DOUBLE_EQ(d1.price, 0.002986);
  EXPECT_DOUBLE_EQ(d1.orig_qty, 0.05);
  EXPECT_DOUBLE_EQ(d1.executed_qty, 0.0);
  EXPECT_DOUBLE_EQ(d1.cummulative_quote_qty, 0.0);
  EXPECT_EQ(d1.status, json::OrderStatus::CANCELED);
  EXPECT_EQ(d1.time_in_force, json::TimeInForce::GTC);
  EXPECT_EQ(d1.type, json::OrderType::LIMIT);
  EXPECT_EQ(d1.side, json::Side::BUY);
}
