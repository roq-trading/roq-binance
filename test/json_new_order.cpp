/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"
#include "roq/core/json/parser.hpp"

#include "roq/binance/json/encoder.hpp"
#include "roq/binance/json/new_order.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::NewOrder;

TEST_CASE("cross", "[json_new_order]") {
  auto message = R"({)"
                 R"("symbol":"BTCUSDT",)"
                 R"("orderId":55329626247,)"
                 R"("clientOrderId":"xwAC9cvtHEwAAQAAAAAA",)"
                 R"("transactTime":1768379896435,)"
                 R"("price":"80000",)"
                 R"("origQty":"0.0001",)"
                 R"("executedQty":"0",)"
                 R"("cummulativeQuoteQty":"0",)"
                 R"("status":"NEW",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("fills":[],)"
                 R"("isIsolated":false,)"
                 R"("selfTradePreventionMode":"EXPIRE_MAKER")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.symbol == "BTCUSDT"sv);
    CHECK(obj.order_id == 55329626247);
  };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}
