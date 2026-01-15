/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"

#include "roq/binance/json/open_orders_ack.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::OpenOrdersAck;

TEST_CASE("empty", "[json_open_orders_ack]") {
  auto message = R"([])"sv;
  auto helper = [&](value_type &obj) { REQUIRE(std::size(obj.data) == 0); };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}

TEST_CASE("simple", "[json_open_orders_ack]") {
  auto message = R"([{)"
                 R"("symbol":"LTCBTC",)"
                 R"("orderId":778507063,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"qQAC6gMAAQAAS-jxw4MW",)"
                 R"("price":"0.00304100",)"
                 R"("origQty":"0.10000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"NEW",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("stopPrice":"0.00000000",)"
                 R"("icebergQty":"0.00000000",)"
                 R"("time":1634214384058,)"
                 R"("updateTime":1634214384058,)"
                 R"("isWorking":true,)"
                 R"("origQuoteOrderQty":"0.00000000")"
                 R"(})"
                 R"(])"sv;
  auto helper = [&](value_type &obj) {
    auto &data = obj.data;
    REQUIRE(std::size(data) == 1);
    auto &data_0 = data[0];
    CHECK(data_0.symbol == "LTCBTC"sv);
    CHECK(data_0.order_id == 778507063);
    CHECK(data_0.order_list_id == -1);
    CHECK(data_0.client_order_id == "qQAC6gMAAQAAS-jxw4MW"sv);
    CHECK(data_0.price == 0.003041_a);
    CHECK(data_0.orig_qty == 0.1_a);
    CHECK(data_0.executed_qty == 0.0_a);
    CHECK(data_0.cummulative_quote_qty == 0.0_a);
    CHECK(data_0.status == json::OrderStatus::NEW);
    CHECK(data_0.time_in_force == json::TimeInForce::GTC);
    CHECK(data_0.type == json::OrderType::LIMIT);
    CHECK(data_0.side == json::Side::BUY);
    CHECK(data_0.stop_price == 0.0_a);
    CHECK(data_0.iceberg_qty == 0.0_a);
    CHECK(data_0.time == 1634214384058ms);
    CHECK(data_0.update_time == 1634214384058ms);
    CHECK(data_0.is_working == true);
    CHECK(data_0.orig_quote_order_qty == 0.0_a);
  };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}
