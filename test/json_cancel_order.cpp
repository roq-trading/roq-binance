/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/cancel_order.h"

using namespace roq;
using namespace roq::binance;

using namespace std::chrono_literals;

TEST(json_cancel_order, simple) {
  auto message = R"({)"
                 R"("symbol":"LTCBTC",)"
                 R"("origClientOrderId":"OgAC6QMAAQAACt7PDZQW",)"
                 R"("orderId":779219002,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"PQAC6QMAAgAAfWABDpQW",)"
                 R"("price":"0.00298200",)"
                 R"("origQty":"0.05000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY")"
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::CancelOrder>(message, buffer);
  EXPECT_EQ(obj.symbol, "LTCBTC"_sv);
  EXPECT_EQ(obj.orig_client_order_id, "OgAC6QMAAQAACt7PDZQW"_sv);
  EXPECT_EQ(obj.order_id, 779219002);
  EXPECT_EQ(obj.order_list_id, -1);
  EXPECT_EQ(obj.client_order_id, "PQAC6QMAAgAAfWABDpQW"_sv);
  EXPECT_DOUBLE_EQ(obj.price, 0.002982);
  EXPECT_DOUBLE_EQ(obj.orig_qty, 0.05);
  EXPECT_DOUBLE_EQ(obj.executed_qty, 0.0);
  EXPECT_DOUBLE_EQ(obj.cummulative_quote_qty, 0.0);
  EXPECT_EQ(obj.status, json::OrderStatus::CANCELED);
  EXPECT_EQ(obj.time_in_force, json::TimeInForce::GTC);
  EXPECT_EQ(obj.type, json::OrderType::LIMIT);
  EXPECT_EQ(obj.side, json::Side::BUY);
}
