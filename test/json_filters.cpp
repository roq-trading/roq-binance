/* Copyright (c) 2017-2025, Hans Erik Thrane */

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
  json::Filters obj{message, buffer};
  auto &data = obj.data;
  REQUIRE(std::size(data) == 8);
  auto &data_0 = data[0];
  CHECK(data_0.filter_type == json::FilterType::PRICE_FILTER);
  CHECK(data_0.min_price == 0.00000001_a);
  CHECK(data_0.max_price == 1000.0_a);
  CHECK(data_0.tick_size == 0.00000001_a);
  auto &data_1 = data[1];
  CHECK(data_1.filter_type == json::FilterType::PERCENT_PRICE);
  CHECK(data_1.multiplier_up == 5.0_a);
  CHECK(data_1.multiplier_down == 0.2_a);
  CHECK(data_1.avg_price_mins == 5.0_a);
  auto &data_2 = data[2];
  CHECK(data_2.filter_type == json::FilterType::LOT_SIZE);
  CHECK(data_2.min_qty == 0.1_a);
  CHECK(data_2.max_qty == 92141578.0_a);
  CHECK(data_2.step_size == 0.1_a);
  auto &data_3 = data[3];
  CHECK(data_3.filter_type == json::FilterType::MIN_NOTIONAL);
  CHECK(data_3.min_notional == 0.0001_a);
  CHECK(data_3.apply_to_market == true);
  CHECK(data_3.avg_price_mins == 5.0_a);
  auto &data_4 = data[4];
  CHECK(data_4.filter_type == json::FilterType::ICEBERG_PARTS);
  CHECK(data_4.limit == 10);
  auto &data_5 = data[5];
  CHECK(data_5.filter_type == json::FilterType::MARKET_LOT_SIZE);
  CHECK(data_5.min_qty == 0.0_a);
  CHECK(data_5.max_qty == 34193.47451388_a);
  CHECK(data_5.step_size == 0.0_a);
  auto &data_6 = data[6];
  CHECK(data_6.filter_type == json::FilterType::MAX_NUM_ORDERS);
  CHECK(data_6.max_num_orders == 200);
  auto &data_7 = data[7];
  CHECK(data_7.filter_type == json::FilterType::MAX_NUM_ALGO_ORDERS);
  CHECK(data_7.max_num_algo_orders == 5);
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
  json::Filters obj{message, buffer};
  auto &data = obj.data;
  REQUIRE(std::size(data) == 9);
  auto &data_0 = data[0];
  CHECK(data_0.filter_type == json::FilterType::PRICE_FILTER);
  CHECK(data_0.min_price == 11.878_a);
  CHECK(data_0.max_price == 225.669_a);
  CHECK(data_0.tick_size == 0.001_a);
  auto &data_1 = data[1];
  CHECK(data_1.filter_type == json::FilterType::PERCENT_PRICE);
  CHECK(data_1.multiplier_up == 5.0_a);
  CHECK(data_1.multiplier_down == 0.2_a);
  CHECK(data_1.avg_price_mins == 5.0_a);
  auto &data_2 = data[2];
  CHECK(data_2.filter_type == json::FilterType::LOT_SIZE);
  CHECK(data_2.min_qty == 0.01_a);
  CHECK(data_2.max_qty == 3000.0_a);
  CHECK(data_2.step_size == 0.01_a);
  auto &data_3 = data[3];
  CHECK(data_3.filter_type == json::FilterType::MIN_NOTIONAL);
  CHECK(data_3.min_notional == 10.0_a);
  CHECK(data_3.apply_to_market == true);
  CHECK(data_3.avg_price_mins == 5.0_a);
  auto &data_4 = data[4];
  CHECK(data_4.filter_type == json::FilterType::ICEBERG_PARTS);
  CHECK(data_4.limit == 10);
  auto &data_5 = data[5];
  CHECK(data_5.filter_type == json::FilterType::MARKET_LOT_SIZE);
  CHECK(data_5.min_qty == 0.0_a);
  CHECK(data_5.max_qty == 3000.0_a);
  CHECK(data_5.step_size == 0.0_a);
  auto &data_6 = data[6];
  CHECK(data_6.filter_type == json::FilterType::MAX_NUM_ORDERS);
  CHECK(data_6.max_num_orders == 200);
  auto &data_7 = data[7];
  CHECK(data_7.filter_type == json::FilterType::MAX_POSITION);
  CHECK(data_7.max_position == 42.0_a);
  auto &data_8 = data[8];
  CHECK(data_8.filter_type == json::FilterType::MAX_NUM_ALGO_ORDERS);
  CHECK(data_8.max_num_algo_orders == 5);
}
