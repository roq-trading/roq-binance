/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/cancel_replace_order.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_cancel_replace_order_simple", "[json_cancel_replace_order]") {
  auto message = R"({)"
                 R"("cancelResult":"SUCCESS",)"
                 R"("newOrderResult":"SUCCESS",)"
                 R"("cancelResponse":{)"
                 R"("symbol":"BTCUSDT",)"
                 R"("origClientOrderId":"CgAC6QMAAQAAvgAMirEt",)"
                 R"("orderId":12166372890,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"PZSf3aT26PBaSacwoUWU9g",)"
                 R"("price":"23238.21000000",)"
                 R"("origQty":"0.00100000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY")"
                 R"(},)"
                 R"("newOrderResponse":{)"
                 R"("symbol":"BTCUSDT",)"
                 R"("orderId":12166375153,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"gwAC6QMAAgAAXFo-irEt",)"
                 R"("transactTime":1659699751935,)"
                 R"("price":"23144.46000000",)"
                 R"("origQty":"0.00100000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"NEW",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("fills":[])"
                 R"(})"
                 R"(})";
  std::vector<std::byte> buffer(8192);
  json::CancelReplaceOrder obj{message, buffer};
  CHECK(obj.cancel_result == json::SuccessOrFailure::SUCCESS);
  CHECK(obj.new_order_result == json::SuccessOrFailure::SUCCESS);
  // cancel order
  auto &cancel = obj.cancel_response;
  CHECK(cancel.symbol == "BTCUSDT"sv);
  CHECK(cancel.orig_client_order_id == "CgAC6QMAAQAAvgAMirEt"sv);
  CHECK(cancel.order_id == 12166372890);
  CHECK(cancel.order_list_id == -1);
  CHECK(cancel.client_order_id == "PZSf3aT26PBaSacwoUWU9g"sv);
  CHECK(cancel.price == 23238.21_a);
  CHECK(cancel.orig_qty == 0.001_a);
  CHECK(cancel.executed_qty == 0.0_a);
  CHECK(cancel.cummulative_quote_qty == 0.0_a);
  CHECK(cancel.status == json::OrderStatus::CANCELED);
  CHECK(cancel.time_in_force == json::TimeInForce::GTC);
  CHECK(cancel.type == json::OrderType::LIMIT);
  CHECK(cancel.side == json::Side::BUY);
  // new order
  auto &new_order = obj.new_order_response;
  CHECK(new_order.symbol == "BTCUSDT"sv);
  CHECK(new_order.order_id == 12166375153);
  CHECK(new_order.order_list_id == -1);
  CHECK(new_order.client_order_id == "gwAC6QMAAgAAXFo-irEt"sv);
  CHECK(new_order.transact_time == 1659699751935ms);
  CHECK(new_order.price == 23144.46_a);
  CHECK(new_order.orig_qty == 0.001_a);
  CHECK(new_order.executed_qty == 0.0_a);
  CHECK(new_order.cummulative_quote_qty == 0.0_a);
  CHECK(new_order.status == json::OrderStatus::NEW);
  CHECK(new_order.time_in_force == json::TimeInForce::GTC);
  CHECK(new_order.type == json::OrderType::LIMIT);
  CHECK(new_order.side == json::Side::BUY);
  REQUIRE(std::empty(new_order.fills));
}
