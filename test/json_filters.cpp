/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/filters.h"

using namespace roq;
using namespace roq::binance;

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
}
