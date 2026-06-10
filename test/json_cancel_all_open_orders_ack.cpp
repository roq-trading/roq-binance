/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"

#include "roq/binance/protocol/json/cancel_all_open_orders_ack.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::CancelAllOpenOrdersAck;

TEST_CASE("empty", "[json_cancel_all_open_orders_ack]") {
  auto message = "[]"sv;
  auto helper = [&](value_type &obj) { REQUIRE(std::size(obj.data) == 0); };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}

TEST_CASE("simple_1", "[json_cancel_all_open_orders_ack]") {
  auto message = R"([{)"
                 R"( "symbol":"LTCBTC",)"
                 R"("origClientOrderId":"QAAC6gMAAQAARzE1LJQW",)"
                 R"("orderId":779222768,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"ya8dFipP337pVE3ogU1JFU",)"
                 R"("price":"0.00297800",)"
                 R"("origQty":"0.05000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY")"
                 R"(})"
                 R"(])"sv;
  auto helper = [&](value_type &obj) {
    auto &data = obj.data;
    REQUIRE(std::size(data) == 1);
    auto &data_0 = data[0];
    CHECK(data_0.symbol == "LTCBTC"sv);
    CHECK(data_0.orig_client_order_id == "QAAC6gMAAQAARzE1LJQW"sv);
    CHECK(data_0.order_id == 779222768);
    CHECK(data_0.order_list_id == -1);
    CHECK(data_0.client_order_id == "ya8dFipP337pVE3ogU1JFU"sv);
    CHECK(data_0.price == 0.002978_a);
    CHECK(data_0.orig_qty == 0.05_a);
    CHECK(data_0.executed_qty == 0.0_a);
    CHECK(data_0.cummulative_quote_qty == 0.0_a);
    CHECK(data_0.status == protocol::json::OrderStatus::CANCELED);
    CHECK(data_0.time_in_force == protocol::json::TimeInForce::GTC);
    CHECK(data_0.type == protocol::json::OrderType::LIMIT);
    CHECK(data_0.side == protocol::json::Side::BUY);
  };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}

TEST_CASE("simple_2", "[json_cancel_all_open_orders_ack]") {
  auto message = R"([{)"
                 R"("symbol":"LTCBTC",)"
                 R"("origClientOrderId":"RAAC6wMAAQAATcQ10ZMW",)"
                 R"("orderId":779213492,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"xsyFwe7R4szBwr2N8UNBii",)"
                 R"("price":"0.00298400",)"
                 R"("origQty":"0.05000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY")"
                 R"(},{)"
                 R"("symbol":"LTCBTC",)"
                 R"("origClientOrderId":"XgAC7AMAAQAAWZxp3JMW",)"
                 R"("orderId":779214518,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"xsyFwe7R4szBwr2N8UNBii",)"
                 R"("price":"0.00298600",)"
                 R"("origQty":"0.05000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY")"
                 R"(})"
                 R"(])"sv;
  auto helper = [&](value_type &obj) {
    auto &data = obj.data;
    REQUIRE(std::size(data) == 2);
    auto &data_0 = data[0];
    CHECK(data_0.symbol == "LTCBTC"sv);
    CHECK(data_0.orig_client_order_id == "RAAC6wMAAQAATcQ10ZMW"sv);
    CHECK(data_0.order_id == 779213492);
    CHECK(data_0.order_list_id == -1);
    CHECK(data_0.client_order_id == "xsyFwe7R4szBwr2N8UNBii"sv);
    CHECK(data_0.price == 0.002984_a);
    CHECK(data_0.orig_qty == 0.05_a);
    CHECK(data_0.executed_qty == 0.0_a);
    CHECK(data_0.cummulative_quote_qty == 0.0_a);
    CHECK(data_0.status == protocol::json::OrderStatus::CANCELED);
    CHECK(data_0.time_in_force == protocol::json::TimeInForce::GTC);
    CHECK(data_0.type == protocol::json::OrderType::LIMIT);
    CHECK(data_0.side == protocol::json::Side::BUY);
    auto &data_1 = data[1];
    CHECK(data_1.symbol == "LTCBTC"sv);
    CHECK(data_1.orig_client_order_id == "XgAC7AMAAQAAWZxp3JMW"sv);
    CHECK(data_1.order_id == 779214518);
    CHECK(data_1.order_list_id == -1);
    CHECK(data_1.client_order_id == "xsyFwe7R4szBwr2N8UNBii"sv);
    CHECK(data_1.price == 0.002986_a);
    CHECK(data_1.orig_qty == 0.05_a);
    CHECK(data_1.executed_qty == 0.0_a);
    CHECK(data_1.cummulative_quote_qty == 0.0_a);
    CHECK(data_1.status == protocol::json::OrderStatus::CANCELED);
    CHECK(data_1.time_in_force == protocol::json::TimeInForce::GTC);
    CHECK(data_1.type == protocol::json::OrderType::LIMIT);
    CHECK(data_1.side == protocol::json::Side::BUY);
  };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}
