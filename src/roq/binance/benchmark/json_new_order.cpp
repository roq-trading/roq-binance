/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <benchmark/benchmark.h>

#include "roq/binance/json/utils.hpp"

#include "roq/binance/tools/crypto.hpp"

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
  result.quantity_precision.decimals = Decimals::_5;
  result.price_precision.decimals = Decimals::_2;
  return result;
}();
}  // namespace
auto const CREATE_ORDER_TEMPLATE = json::CreateOrderTemplate{};

// === IMPLEMENTATION ===

// --- json_new_order ---

void BM_json_new_order(benchmark::State &state) {
  std::vector<char> buffer(4096);
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
        .request_template = {},
        .quantity = QUANTITY,
        .price = PRICE,
        .stop_price = NaN,
        .routing_id = {},
        .strategy_id = {},
    };
    json::new_order(buffer, create_order, OMS_ORDER, REQUEST_ID, CREATE_ORDER_TEMPLATE, RECV_WINDOW);
  }
}

BENCHMARK(BM_json_new_order);

// --- json_new_order_with_signature ---

void BM_json_new_order_with_signature(benchmark::State &state) {
  std::vector<char> buffer(4096);
  std::vector<std::byte> buffer_2(tools::Crypto::QUERY_BUFFER_LENGTH);
  tools::Crypto crypto{KEY, SECRET};
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
        .request_template = {},
        .quantity = QUANTITY,
        .price = PRICE,
        .stop_price = NaN,
        .routing_id = {},
        .strategy_id = {},
    };
    auto body = json::new_order(buffer, create_order, OMS_ORDER, REQUEST_ID, CREATE_ORDER_TEMPLATE, RECV_WINDOW);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    crypto.create_query(buffer_2, now, body);
  }
}

BENCHMARK(BM_json_new_order_with_signature);
