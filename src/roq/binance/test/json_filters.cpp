/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/filters.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_filters_simple_1", "[json_filters]") {
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
  std::vector<std::byte> buffer(8192);
  auto obj = json::Filters::create(message, buffer);
  auto &data = obj.data;
  REQUIRE(std::size(data) == 8);
  auto &d0 = data[0];
  CHECK(d0.filter_type == json::FilterType::PRICE_FILTER);
  CHECK(d0.min_price == 0.00000001_a);
  CHECK(d0.max_price == 1000.0_a);
  CHECK(d0.tick_size == 0.00000001_a);
  auto &d1 = data[1];
  CHECK(d1.filter_type == json::FilterType::PERCENT_PRICE);
  CHECK(d1.multiplier_up == 5.0_a);
  CHECK(d1.multiplier_down == 0.2_a);
  CHECK(d1.avg_price_mins == 5.0_a);
  auto &d2 = data[2];
  CHECK(d2.filter_type == json::FilterType::LOT_SIZE);
  CHECK(d2.min_qty == 0.1_a);
  CHECK(d2.max_qty == 92141578.0_a);
  CHECK(d2.step_size == 0.1_a);
  auto &d3 = data[3];
  CHECK(d3.filter_type == json::FilterType::MIN_NOTIONAL);
  CHECK(d3.min_notional == 0.0001_a);
  CHECK(d3.apply_to_market == true);
  CHECK(d3.avg_price_mins == 5.0_a);
  auto &d4 = data[4];
  CHECK(d4.filter_type == json::FilterType::ICEBERG_PARTS);
  CHECK(d4.limit == 10);
  auto &d5 = data[5];
  CHECK(d5.filter_type == json::FilterType::MARKET_LOT_SIZE);
  CHECK(d5.min_qty == 0.0_a);
  CHECK(d5.max_qty == 34193.47451388_a);
  CHECK(d5.step_size == 0.0_a);
  auto &d6 = data[6];
  CHECK(d6.filter_type == json::FilterType::MAX_NUM_ORDERS);
  CHECK(d6.max_num_orders == 200);
  auto &d7 = data[7];
  CHECK(d7.filter_type == json::FilterType::MAX_NUM_ALGO_ORDERS);
  CHECK(d7.max_num_algo_orders == 5);
}

TEST_CASE("json_filters_simple_2", "[json_filters]") {
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
  std::vector<std::byte> buffer(8192);
  auto obj = json::Filters::create(message, buffer);
  auto &data = obj.data;
  REQUIRE(std::size(data) == 9);
  auto &d0 = data[0];
  CHECK(d0.filter_type == json::FilterType::PRICE_FILTER);
  CHECK(d0.min_price == 11.878_a);
  CHECK(d0.max_price == 225.669_a);
  CHECK(d0.tick_size == 0.001_a);
  auto &d1 = data[1];
  CHECK(d1.filter_type == json::FilterType::PERCENT_PRICE);
  CHECK(d1.multiplier_up == 5.0_a);
  CHECK(d1.multiplier_down == 0.2_a);
  CHECK(d1.avg_price_mins == 5.0_a);
  auto &d2 = data[2];
  CHECK(d2.filter_type == json::FilterType::LOT_SIZE);
  CHECK(d2.min_qty == 0.01_a);
  CHECK(d2.max_qty == 3000.0_a);
  CHECK(d2.step_size == 0.01_a);
  auto &d3 = data[3];
  CHECK(d3.filter_type == json::FilterType::MIN_NOTIONAL);
  CHECK(d3.min_notional == 10.0_a);
  CHECK(d3.apply_to_market == true);
  CHECK(d3.avg_price_mins == 5.0_a);
  auto &d4 = data[4];
  CHECK(d4.filter_type == json::FilterType::ICEBERG_PARTS);
  CHECK(d4.limit == 10);
  auto &d5 = data[5];
  CHECK(d5.filter_type == json::FilterType::MARKET_LOT_SIZE);
  CHECK(d5.min_qty == 0.0_a);
  CHECK(d5.max_qty == 3000.0_a);
  CHECK(d5.step_size == 0.0_a);
  auto &d6 = data[6];
  CHECK(d6.filter_type == json::FilterType::MAX_NUM_ORDERS);
  CHECK(d6.max_num_orders == 200);
  auto &d7 = data[7];
  CHECK(d7.filter_type == json::FilterType::MAX_POSITION);
  CHECK(d7.max_position == 42.0_a);
  auto &d8 = data[8];
  CHECK(d8.filter_type == json::FilterType::MAX_NUM_ALGO_ORDERS);
  CHECK(d8.max_num_algo_orders == 5);
}
