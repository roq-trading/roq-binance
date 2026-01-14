/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"
#include "roq/core/json/parser.hpp"

#include "roq/binance/json/cancel_order.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::CancelOrder;

TEST_CASE("cross", "[json_cancel_order]") {
  auto message = R"({)"
                 R"("orderId":"55329759393",)"
                 R"("symbol":"BTCUSDT",)"
                 R"("origClientOrderId":"ZgAC8K8sHUwAAQAAAAAA",)"
                 R"("clientOrderId":"ZQAC8K8sHUwAAgAAAAAA",)"
                 R"("transactTime":1768380307534,)"
                 R"("price":"80000",)"
                 R"("origQty":"0.0001",)"
                 R"("executedQty":"0",)"
                 R"("cummulativeQuoteQty":"0",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("isIsolated":false,)"
                 R"("selfTradePreventionMode":"EXPIRE_MAKER")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.order_id == 55329759393);
    CHECK(obj.symbol == "BTCUSDT"sv);
  };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}
