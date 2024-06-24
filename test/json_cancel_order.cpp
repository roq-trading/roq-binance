/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/cancel_order.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_cancel_order_simple", "[json_cancel_order]") {
  auto message = R"({)"
                 R"("symbol":"LTCBTC",)"
                 R"("origClientOrderId":"OgAC6QMAAQAACt7PDZQW",)"
                 R"("orderId":779219002,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"PQAC6QMAAgAAfWABDpQW",)"
                 R"("price":"0.00298200",)"
                 R"("origQty":"0.05000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY")"
                 R"(})";
  std::vector<std::byte> buffer(8192);
  json::CancelOrder obj{message, buffer};
  CHECK(obj.symbol == "LTCBTC"sv);
  CHECK(obj.orig_client_order_id == "OgAC6QMAAQAACt7PDZQW"sv);
  CHECK(obj.order_id == 779219002);
  CHECK(obj.order_list_id == -1);
  CHECK(obj.client_order_id == "PQAC6QMAAgAAfWABDpQW"sv);
  CHECK(obj.price == 0.002982_a);
  CHECK(obj.orig_qty == 0.05_a);
  CHECK(obj.executed_qty == 0.0_a);
  CHECK(obj.cummulative_quote_qty == 0.0_a);
  CHECK(obj.status == json::OrderStatus::CANCELED);
  CHECK(obj.time_in_force == json::TimeInForce::GTC);
  CHECK(obj.type == json::OrderType::LIMIT);
  CHECK(obj.side == json::Side::BUY);
}
