/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/logging.hpp"

#include "roq/binance/json/encoder.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

// === HELPERS ===

namespace {
auto create_create_order(double quantity, double price) {
  return CreateOrder{
      .account = "A1"sv,
      .order_id = 1234,
      .exchange = "binance"sv,
      .symbol = "BTC"sv,
      .side = Side::BUY,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = NaN,
      .order_type = OrderType::LIMIT,
      .time_in_force = TimeInForce::GTC,
      .execution_instructions = {},
      .request_template = {},
      .quantity = quantity,
      .price = price,
      .stop_price = NaN,
      .leverage = NaN,
      .routing_id = {},
      .strategy_id = {},
  };
}

auto create_order(double quantity, double price, std::string_view const &external_order_id) {
  auto order = server::oms::Order{
      .user_id = {},
      .stream_id = {},
      .account = {},
      .order_id = {},
      .exchange = "binance"sv,
      .symbol = "BTC"sv,
      .side = Side::BUY,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = {},
      .order_type = {},
      .time_in_force = {},
      .execution_instructions = {},
      .create_time_utc = {},
      .update_time_utc = {},
      .external_account = {},
      .external_order_id = external_order_id,
      .client_order_id = {},
      .order_status = {},
      .quantity = quantity,
      .price = price,
      .stop_price = {},
      .leverage = NaN,
      .risk_exposure = {},
      .remaining_quantity = {},
      .traded_quantity = {},
      .average_traded_price = {},
      .last_traded_price = {},
      .last_traded_quantity = {},
      .last_liquidity = {},
      .routing_id = {},
      .max_request_version = {},
      .max_response_version = {},
      .max_accepted_version = {},
      .security_type = {},
      .quantity_precision = {},
      .price_precision = {},
      .update_type = {},
      .user = {},
      .strategy_id = {},
  };
  return order;
}
}  // namespace

// === IMPLEMENTATION ===

TEST_CASE("create_json", "[json_encoder]") {
  std::vector<char> buffer;
  auto create_order_2 = create_create_order(1.0, 1.0);
  auto order = create_order(1.0, 1.0, {});
  auto request_id = "oid:1234"sv;
  json::CreateOrderTemplate create_order_template;
  auto result = json::Encoder::place_order_json(buffer, create_order_2, order, request_id, create_order_template, 5s, {});
  CHECK(
      result == R"({)"
                R"("newClientOrderId":"oid:1234",)"
                R"("price":"1",)"
                R"("quantity":"1",)"
                R"("recvWindow":5000,)"
                R"("side":"BUY",)"
                R"("symbol":"BTC",)"
                R"("timeInForce":"GTC",)"
                R"("timestamp":0,)"
                R"("type":"LIMIT")"
                R"(})"sv);
}
