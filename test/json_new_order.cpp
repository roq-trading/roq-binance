/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/new_order.h"

using namespace roq;
using namespace roq::binance;

using namespace std::chrono_literals;

TEST(json_new_order, simple) {
  auto message = R"({)"
                 R"("symbol":"LTCBTC",)"
                 R"("orderId":778507063,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"qQAC6gMAAQAAS-jxw4MW",)"
                 R"("transactTime":1634214384058,)"
                 R"("price":"0.00304100",)"
                 R"("origQty":"0.10000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"NEW",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("fills":[])"
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::NewOrder>(message, buffer);
  EXPECT_EQ(obj.symbol, "LTCBTC"_sv);
  EXPECT_EQ(obj.order_id, 778507063);
  EXPECT_EQ(obj.order_list_id, -1);
  EXPECT_EQ(obj.client_order_id, "qQAC6gMAAQAAS-jxw4MW"_sv);
  EXPECT_EQ(obj.transact_time, 1634214384058ms);
  EXPECT_DOUBLE_EQ(obj.price, 0.003041);
  EXPECT_DOUBLE_EQ(obj.orig_qty, 0.1);
  EXPECT_DOUBLE_EQ(obj.cummulative_quote_qty, 0.0);
  EXPECT_EQ(obj.status, json::OrderStatus::NEW);
  EXPECT_EQ(obj.time_in_force, json::TimeInForce::GTC);
  EXPECT_EQ(obj.type, json::OrderType::LIMIT);
  EXPECT_EQ(obj.side, json::Side::BUY);
  EXPECT_EQ(std::size(obj.fills), 0);
}
