/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "wsapi_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::WSAPICancelOpenOrders;

TEST_CASE("empty", "[json_wsapi_cancel_open_orders]") {
  auto message = R"({)"
                 R"("id":"iIQeAAoCAAAAAAAAAAAAAAAAAAAAAAAAAAAA",)"
                 R"("status":200,)"
                 R"("result":[{)"
                 R"("symbol":"BTCUSDT",)"
                 R"("origClientOrderId":"sQACRXQ9tUwAAQAAAAAA",)"
                 R"("orderId":55471228235,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"cwA1HA2K0sp6VYm9mkQHMP",)"
                 R"("transactTime":1768635430921,)"
                 R"("price":"80000.00000000",)"
                 R"("origQty":"0.00010000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("origQuoteOrderQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("selfTradePreventionMode":"EXPIRE_MAKER")"
                 R"(})"
                 R"(],)"
                 R"("rateLimits":[{)"
                 R"("rateLimitType":"REQUEST_WEIGHT",)"
                 R"("interval":"MINUTE",)"
                 R"("intervalNum":1,)"
                 R"("limit":6000,)"
                 R"("count":172)"
                 R"(})"
                 R"(])"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.id == "iIQeAAoCAAAAAAAAAAAAAAAAAAAAAAAAAAAA"sv);
    CHECK(obj.status == 200);
  };
  WSAPIParserTester<value_type>::dispatch(helper, message, 65536, 1);
}
