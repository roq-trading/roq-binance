/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/new_order.h"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
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
  EXPECT_EQ(obj.symbol, "LTCBTC"sv);
  EXPECT_EQ(obj.order_id, 778507063);
  EXPECT_EQ(obj.order_list_id, -1);
  EXPECT_EQ(obj.client_order_id, "qQAC6gMAAQAAS-jxw4MW"sv);
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

TEST(json_new_order, simple_maker) {
  auto message = R"({)"
                 R"("symbol":"LTCUSDT",)"
                 R"("orderId":2426862755,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"SwAC6wMAAQAA8foJ1iQX",)"
                 R"("transactTime":1634906177360,)"
                 R"("price":"198.30000000",)"
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
  EXPECT_EQ(obj.symbol, "LTCUSDT"sv);
  EXPECT_EQ(obj.order_id, 2426862755);
  EXPECT_EQ(obj.order_list_id, -1);
  EXPECT_EQ(obj.client_order_id, "SwAC6wMAAQAA8foJ1iQX"sv);
  EXPECT_EQ(obj.transact_time, 1634906177360ms);
  EXPECT_DOUBLE_EQ(obj.price, 198.3);
  EXPECT_DOUBLE_EQ(obj.orig_qty, 0.1);
  EXPECT_DOUBLE_EQ(obj.cummulative_quote_qty, 0.0);
  EXPECT_EQ(obj.status, json::OrderStatus::NEW);
  EXPECT_EQ(obj.time_in_force, json::TimeInForce::GTC);
  EXPECT_EQ(obj.type, json::OrderType::LIMIT);
  EXPECT_EQ(obj.side, json::Side::BUY);
  auto &fills = obj.fills;
  EXPECT_EQ(std::size(fills), 0);
}

TEST(json_new_order, simple_taker) {
  auto message = R"({)"
                 R"("symbol":"LTCUSDT",)"
                 R"("orderId":2426923399,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"bQAC6QMAAQAAaNElbSUX",)"
                 R"("transactTime":1634908712538,)"
                 R"("price":"198.50000000",)"
                 R"("origQty":"0.10000000",)"
                 R"("executedQty":"0.10000000",)"
                 R"("cummulativeQuoteQty":"19.85000000",)"
                 R"("status":"FILLED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"SELL",)"
                 R"("fills":[{)"
                 R"("price":"198.50000000",)"
                 R"("qty":"0.10000000",)"
                 R"("commission":"0.00003030",)"
                 R"("commissionAsset":"BNB",)"
                 R"("tradeId":207085107)"
                 R"(})"
                 R"(])"
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::NewOrder>(message, buffer);
  EXPECT_EQ(obj.symbol, "LTCUSDT"sv);
  EXPECT_EQ(obj.order_id, 2426923399);
  EXPECT_EQ(obj.order_list_id, -1);
  EXPECT_EQ(obj.client_order_id, "bQAC6QMAAQAAaNElbSUX"sv);
  EXPECT_EQ(obj.transact_time, 1634908712538ms);
  EXPECT_DOUBLE_EQ(obj.price, 198.5);
  EXPECT_DOUBLE_EQ(obj.orig_qty, 0.1);
  EXPECT_DOUBLE_EQ(obj.cummulative_quote_qty, 19.85);
  EXPECT_EQ(obj.status, json::OrderStatus::FILLED);
  EXPECT_EQ(obj.time_in_force, json::TimeInForce::GTC);
  EXPECT_EQ(obj.type, json::OrderType::LIMIT);
  EXPECT_EQ(obj.side, json::Side::SELL);
  auto &fills = obj.fills;
  EXPECT_EQ(std::size(fills), 1);
  auto &f0 = fills[0];
  EXPECT_DOUBLE_EQ(f0.price, 198.5);
  EXPECT_DOUBLE_EQ(f0.qty, 0.1);
  EXPECT_DOUBLE_EQ(f0.commission, 0.0000303);
  EXPECT_EQ(f0.commission_asset, "BNB"sv);
  EXPECT_EQ(f0.trade_id, 207085107);
}
