/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/filters.h"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

TEST(json_filters, simple_1) {
  auto message = R"([{)"
                 R"("filterType":"PRICE_FILTER",)"
                 R"("minPrice":"0.00000001",)"
                 R"("maxPrice":"1000.00000000",)"
                 R"("tickSize":"0.00000001")"
                 R"(},{)"
                 R"("filterType":"PERCENT_PRICE",)"
                 R"("multiplierUp":"5",)"
                 R"("multiplierDown":"0.2",)"
                 R"("avgPriceMins":5)"
                 R"(},{)"
                 R"("filterType":"LOT_SIZE",)"
                 R"("minQty":"0.10000000",)"
                 R"("maxQty":"92141578.00000000",)"
                 R"("stepSize":"0.10000000")"
                 R"(},{)"
                 R"("filterType":"MIN_NOTIONAL",)"
                 R"("minNotional":"0.00010000",)"
                 R"("applyToMarket":true,)"
                 R"("avgPriceMins":5)"
                 R"(},{)"
                 R"("filterType":"ICEBERG_PARTS",)"
                 R"("limit":10)"
                 R"(},{)"
                 R"("filterType":"MARKET_LOT_SIZE",)"
                 R"("minQty":"0.00000000",)"
                 R"("maxQty":"34193.47451388",)"
                 R"("stepSize":"0.00000000")"
                 R"(},{)"
                 R"("filterType":"MAX_NUM_ORDERS",)"
                 R"("maxNumOrders":200)"
                 R"(},{)"
                 R"("filterType":"MAX_NUM_ALGO_ORDERS",)"
                 R"("maxNumAlgoOrders":5)"
                 R"(})"
                 R"(])";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::Filters>(message, buffer);
  auto &data = obj.data;
  ASSERT_EQ(std::size(data), 8);
  auto &d0 = data[0];
  EXPECT_EQ(d0.filter_type, json::FilterType::PRICE_FILTER);
  EXPECT_DOUBLE_EQ(d0.min_price, 0.00000001);
  EXPECT_DOUBLE_EQ(d0.max_price, 1000.0);
  EXPECT_DOUBLE_EQ(d0.tick_size, 0.00000001);
  auto &d1 = data[1];
  EXPECT_EQ(d1.filter_type, json::FilterType::PERCENT_PRICE);
  EXPECT_DOUBLE_EQ(d1.multiplier_up, 5.0);
  EXPECT_DOUBLE_EQ(d1.multiplier_down, 0.2);
  EXPECT_DOUBLE_EQ(d1.avg_price_mins, 5.0);
  auto &d2 = data[2];
  EXPECT_EQ(d2.filter_type, json::FilterType::LOT_SIZE);
  EXPECT_DOUBLE_EQ(d2.min_qty, 0.1);
  EXPECT_DOUBLE_EQ(d2.max_qty, 92141578.0);
  EXPECT_DOUBLE_EQ(d2.step_size, 0.1);
  auto &d3 = data[3];
  EXPECT_EQ(d3.filter_type, json::FilterType::MIN_NOTIONAL);
  EXPECT_DOUBLE_EQ(d3.min_notional, 0.0001);
  EXPECT_EQ(d3.apply_to_market, true);
  EXPECT_DOUBLE_EQ(d3.avg_price_mins, 5.0);
  auto &d4 = data[4];
  EXPECT_EQ(d4.filter_type, json::FilterType::ICEBERG_PARTS);
  EXPECT_EQ(d4.limit, 10);
  auto &d5 = data[5];
  EXPECT_EQ(d5.filter_type, json::FilterType::MARKET_LOT_SIZE);
  EXPECT_DOUBLE_EQ(d5.min_qty, 0.0);
  EXPECT_DOUBLE_EQ(d5.max_qty, 34193.47451388);
  EXPECT_DOUBLE_EQ(d5.step_size, 0.0);
  auto &d6 = data[6];
  EXPECT_EQ(d6.filter_type, json::FilterType::MAX_NUM_ORDERS);
  EXPECT_EQ(d6.max_num_orders, 200);
  auto &d7 = data[7];
  EXPECT_EQ(d7.filter_type, json::FilterType::MAX_NUM_ALGO_ORDERS);
  EXPECT_EQ(d7.max_num_algo_orders, 5);
}

TEST(json_filters, simple_2) {
  auto message = R"([{)"
                 R"("filterType":"PRICE_FILTER",)"
                 R"("minPrice":"11.87800000",)"
                 R"("maxPrice":"225.66900000",)"
                 R"("tickSize":"0.00100000")"
                 R"(},{)"
                 R"("filterType":"PERCENT_PRICE",)"
                 R"("multiplierUp":"5",)"
                 R"("multiplierDown":"0.2",)"
                 R"("avgPriceMins":5)"
                 R"(},{)"
                 R"("filterType":"LOT_SIZE",)"
                 R"("minQty":"0.01000000",)"
                 R"("maxQty":"3000.00000000",)"
                 R"("stepSize":"0.01000000")"
                 R"(},{)"
                 R"("filterType":"MIN_NOTIONAL",)"
                 R"("minNotional":"10.00000000",)"
                 R"("applyToMarket":true,)"
                 R"("avgPriceMins":5)"
                 R"(},{)"
                 R"("filterType":"ICEBERG_PARTS",)"
                 R"("limit":10)"
                 R"(},{)"
                 R"("filterType":"MARKET_LOT_SIZE",)"
                 R"("minQty":"0.00000000",)"
                 R"("maxQty":"3000.00000000",)"
                 R"("stepSize":"0.00000000")"
                 R"(},{)"
                 R"("filterType":"MAX_NUM_ORDERS",)"
                 R"("maxNumOrders":200)"
                 R"(},{)"
                 R"("filterType":"MAX_POSITION",)"
                 R"("maxPosition":"42.00000000")"
                 R"(},{)"
                 R"("filterType":"MAX_NUM_ALGO_ORDERS",)"
                 R"("maxNumAlgoOrders":5)"
                 R"(})"
                 R"(])";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::Filters>(message, buffer);
  auto &data = obj.data;
  ASSERT_EQ(std::size(data), 9);
  auto &d0 = data[0];
  EXPECT_EQ(d0.filter_type, json::FilterType::PRICE_FILTER);
  EXPECT_DOUBLE_EQ(d0.min_price, 11.878);
  EXPECT_DOUBLE_EQ(d0.max_price, 225.669);
  EXPECT_DOUBLE_EQ(d0.tick_size, 0.001);
  auto &d1 = data[1];
  EXPECT_EQ(d1.filter_type, json::FilterType::PERCENT_PRICE);
  EXPECT_DOUBLE_EQ(d1.multiplier_up, 5.0);
  EXPECT_DOUBLE_EQ(d1.multiplier_down, 0.2);
  EXPECT_DOUBLE_EQ(d1.avg_price_mins, 5.0);
  auto &d2 = data[2];
  EXPECT_EQ(d2.filter_type, json::FilterType::LOT_SIZE);
  EXPECT_DOUBLE_EQ(d2.min_qty, 0.01);
  EXPECT_DOUBLE_EQ(d2.max_qty, 3000.0);
  EXPECT_DOUBLE_EQ(d2.step_size, 0.01);
  auto &d3 = data[3];
  EXPECT_EQ(d3.filter_type, json::FilterType::MIN_NOTIONAL);
  EXPECT_DOUBLE_EQ(d3.min_notional, 10.0);
  EXPECT_EQ(d3.apply_to_market, true);
  EXPECT_DOUBLE_EQ(d3.avg_price_mins, 5.0);
  auto &d4 = data[4];
  EXPECT_EQ(d4.filter_type, json::FilterType::ICEBERG_PARTS);
  EXPECT_EQ(d4.limit, 10);
  auto &d5 = data[5];
  EXPECT_EQ(d5.filter_type, json::FilterType::MARKET_LOT_SIZE);
  EXPECT_DOUBLE_EQ(d5.min_qty, 0.0);
  EXPECT_DOUBLE_EQ(d5.max_qty, 3000.0);
  EXPECT_DOUBLE_EQ(d5.step_size, 0.0);
  auto &d6 = data[6];
  EXPECT_EQ(d6.filter_type, json::FilterType::MAX_NUM_ORDERS);
  EXPECT_EQ(d6.max_num_orders, 200);
  auto &d7 = data[7];
  EXPECT_EQ(d7.filter_type, json::FilterType::MAX_POSITION);
  EXPECT_DOUBLE_EQ(d7.max_position, 42.0);
  auto &d8 = data[8];
  EXPECT_EQ(d8.filter_type, json::FilterType::MAX_NUM_ALGO_ORDERS);
  EXPECT_EQ(d8.max_num_algo_orders, 5);
}
