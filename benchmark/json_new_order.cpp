/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <benchmark/benchmark.h>

#include "roq/binance/json/utils.hpp"

#include "roq/binance/tools/hasher.hpp"

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
auto const ORDER_ID = uint32_t{1234};
auto const QUANTITY = 123.45678;
auto const PRICE = 16833.45;
auto const RECV_WINDOW = 5s;
auto const KEY = "sSzUA6j8tGDfmLoFrOPhWHY3VeXbC3NrApp94Ci4H4XvcjuCuvOXp8gH89XzMPDe"sv;
auto const SECRET = "tHurnNFWLFkm97xVRqoESdujAiq1ilNjnY52tDej5RilUbTVZXT2YB5eo7txFLHk"sv;
auto const OMS_ORDER = []() {
  oms::Order result;
  result.quantity_decimals = Decimals::_5;
  result.price_decimals = Decimals::_2;
  return result;
}();
}  // namespace

// === IMPLEMENTATION ===

// --- json_new_order ---

void BM_json_new_order(benchmark::State &state) {
  std::vector<char> buffer(4096);
  uint64_t processed = 0;
  for (auto _ : state) {
    auto create_order = CreateOrder{
        .account = ACCOUNT,
        .order_id = ORDER_ID,
        .exchange = EXCHANGE,
        .symbol = SYMBOL,
        .side = Side::BUY,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = OrderType::LIMIT,
        .time_in_force = TimeInForce::GTC,
        .execution_instructions = {},
        .order_template = {},
        .quantity = QUANTITY,
        .price = PRICE,
        .stop_price = NaN,
        .routing_id = {},
    };
    auto body = json::new_order(buffer, create_order, OMS_ORDER, REQUEST_ID, RECV_WINDOW);
    if (!std::empty(body))
      ++processed;
  }
}

BENCHMARK(BM_json_new_order);

// --- json_new_order_with_signature ---

void BM_json_new_order_with_signature(benchmark::State &state) {
  std::vector<char> buffer(4096);
  std::vector<char> buffer_2(4096);
  tools::Hasher hasher{KEY, SECRET};
  uint64_t processed = 0;
  for (auto _ : state) {
    auto create_order = CreateOrder{
        .account = ACCOUNT,
        .order_id = ORDER_ID,
        .exchange = EXCHANGE,
        .symbol = SYMBOL,
        .side = Side::BUY,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = OrderType::LIMIT,
        .time_in_force = TimeInForce::GTC,
        .execution_instructions = {},
        .order_template = {},
        .quantity = QUANTITY,
        .price = PRICE,
        .stop_price = NaN,
        .routing_id = {},
    };
    auto body = json::new_order(buffer, create_order, OMS_ORDER, REQUEST_ID, RECV_WINDOW);
    core::Message<char> message{buffer_2};
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto query = hasher.create_query(message, now, body);
    if (!std::empty(query))
      ++processed;
  }
}

BENCHMARK(BM_json_new_order_with_signature);
