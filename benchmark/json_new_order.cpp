/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <benchmark/benchmark.h>

#include "roq/binance/json/utils.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

// === CONSTANTS ===

namespace {
auto const REQUEST_ID = "jQAB6gMAAQAAQUIp3sUSAawljiyfnylc"sv;
auto const ACCOUNT = "A1"sv;
auto const EXCHANGE = "binance"sv;
auto const SYMBOL = "BTCUSDT"sv;
auto const recv_window = 5s;
}  // namespace

// === HELPERS ===

namespace {
auto create_oms_order() {
  oms::Order result;
  result.price_decimals = Decimals::_1;
  result.quantity_decimals = Decimals::_2;
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

void BM_json_new_order(benchmark::State &state) {
  std::vector<char> buffer(4096);
  uint64_t processed = 0;
  for (auto _ : state) {
    auto create_order = CreateOrder{
        .account = ACCOUNT,
        .order_id = 1234,
        .exchange = EXCHANGE,
        .symbol = SYMBOL,
        .side = Side::BUY,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = OrderType::LIMIT,
        .time_in_force = TimeInForce::GTC,
        .execution_instructions = {},
        .order_template = {},
        .quantity = 123.45,
        .price = 16833.45,
        .stop_price = NaN,
        .routing_id = {},
    };
    auto order = create_oms_order();
    auto body = json::new_order(buffer, create_order, order, REQUEST_ID, recv_window);
    if (!std::empty(body))
      ++processed;
  }
}

BENCHMARK(BM_json_new_order);
