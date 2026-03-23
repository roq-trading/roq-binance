/* Copyright (c) 2017-2026, Hans Erik Thrane */

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
auto const PREVIOUS_REQUEST_ID = "jQAB6gMAAQAAQUIp3sUSAawljiyfnylc"sv;
auto const ACCOUNT = "A1"sv;
uint32_t const ORDER_ID = 1234;
auto const CANCEL_ORDER_TEMPLATE = json::CancelOrderTemplate{};
auto const RECV_WINDOW = 5s;
auto const KEY = "sSzUA6j8tGDfmLoFrOPhWHY3VeXbC3NrApp94Ci4H4XvcjuCuvOXp8gH89XzMPDe"sv;
auto const SECRET = "tHurnNFWLFkm97xVRqoESdujAiq1ilNjnY52tDej5RilUbTVZXT2YB5eo7txFLHk"sv;
auto const OMS_ORDER = []() {
  server::oms::Order result;
  result.quantity_precision.precision = Precision::_5;
  result.price_precision.precision = Precision::_2;
  return result;
}();
}  // namespace

// === IMPLEMENTATION ===

// --- json_cancel_order --

void BM_json_cancel_order(benchmark::State &state) {
  std::vector<char> buffer(4096);
  for (auto _ : state) {
    auto cancel_order = CancelOrder{
        .account = ACCOUNT,
        .order_id = ORDER_ID,
        .request_template = {},
        .routing_id = {},
        .version = {},
        .conditional_on_version = {},
        .release_time_utc = {},
    };
    json::cancel_order(buffer, cancel_order, OMS_ORDER, REQUEST_ID, PREVIOUS_REQUEST_ID, CANCEL_ORDER_TEMPLATE, RECV_WINDOW);
  }
}

BENCHMARK(BM_json_cancel_order);

// --- json_cancel_order_with_signature --

void BM_json_cancel_order_with_signature(benchmark::State &state) {
  std::vector<char> buffer(4096);
  std::vector<std::byte> buffer_2(tools::Crypto::QUERY_BUFFER_LENGTH);
  tools::Crypto crypto{KEY, SECRET};
  for (auto _ : state) {
    auto cancel_order = CancelOrder{
        .account = ACCOUNT,
        .order_id = ORDER_ID,
        .request_template = {},
        .routing_id = {},
        .version = {},
        .conditional_on_version = {},
        .release_time_utc = {},
    };
    auto body = json::cancel_order(buffer, cancel_order, OMS_ORDER, REQUEST_ID, PREVIOUS_REQUEST_ID, CANCEL_ORDER_TEMPLATE, RECV_WINDOW);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    crypto.create_query(buffer_2, now, body);
  }
}

BENCHMARK(BM_json_cancel_order_with_signature);
