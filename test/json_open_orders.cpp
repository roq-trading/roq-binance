/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/open_orders.h"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

TEST(json_open_orders, simple_empty) {
  auto message = R"([])";
  core::Buffer buffer_(8192);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::OpenOrders>(message, buffer);
  ASSERT_EQ(std::size(obj.data), 0);
}

TEST(json_open_orders, simple) {
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
  ASSERT_EQ(std::size(data), 1);
  auto &d0 = data[0];
  EXPECT_EQ(d0.symbol, "LTCBTC"sv);
  EXPECT_EQ(d0.order_id, 778507063);
  EXPECT_EQ(d0.order_list_id, -1);
  EXPECT_EQ(d0.client_order_id, "qQAC6gMAAQAAS-jxw4MW"sv);
  EXPECT_DOUBLE_EQ(d0.price, 0.003041);
  EXPECT_DOUBLE_EQ(d0.orig_qty, 0.1);
  EXPECT_DOUBLE_EQ(d0.executed_qty, 0.0);
  EXPECT_DOUBLE_EQ(d0.cummulative_quote_qty, 0.0);
  EXPECT_EQ(d0.status, json::OrderStatus::NEW);
  EXPECT_EQ(d0.time_in_force, json::TimeInForce::GTC);
  EXPECT_EQ(d0.type, json::OrderType::LIMIT);
  EXPECT_EQ(d0.side, json::Side::BUY);
  EXPECT_DOUBLE_EQ(d0.stop_price, 0.0);
  EXPECT_DOUBLE_EQ(d0.iceberg_qty, 0.0);
  EXPECT_EQ(d0.time, 1634214384058ms);
  EXPECT_EQ(d0.update_time, 1634214384058ms);
  EXPECT_TRUE(d0.is_working);
  EXPECT_DOUBLE_EQ(d0.orig_quote_order_qty, 0.0);
}
