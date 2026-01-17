/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "wsapi_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::WSAPIOrderPlace;

TEST_CASE("empty", "[json_wsapi_order_place]") {
  auto message = R"({)"
                 R"("id":"h4QeAAcCYCgVtUwAAAABAAAAAAAAAAAAAAAA",)"
                 R"("status":200,)"
                 R"("result":{)"
                 R"("symbol":"BTCUSDT",)"
                 R"("orderId":55471195855,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"4AACYCgVtUwAAQAAAAAA",)"
                 R"("transactTime":1768635167519,)"
                 R"("price":"80000.00000000",)"
                 R"("origQty":"0.00010000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("origQuoteOrderQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"NEW",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("workingTime":1768635167519,)"
                 R"("fills":[],)"
                 R"("selfTradePreventionMode":"EXPIRE_MAKER"},)"
                 R"("rateLimits":[{)"
                 R"("rateLimitType":"ORDERS",)"
                 R"("interval":"SECOND",)"
                 R"("intervalNum":10,)"
                 R"("limit":100,)"
                 R"("count":1)"
                 R"(},{)"
                 R"("rateLimitType":"ORDERS",)"
                 R"("interval":"DAY",)"
                 R"("intervalNum":1,)"
                 R"("limit":200000,)"
                 R"("count":2)"
                 R"(},{)"
                 R"("rateLimitType":"REQUEST_WEIGHT",)"
                 R"("interval":"MINUTE",)"
                 R"("intervalNum":1,)"
                 R"("limit":6000,)"
                 R"("count":546)"
                 R"(})"
                 R"(])"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.id == "h4QeAAcCYCgVtUwAAAABAAAAAAAAAAAAAAAA"sv);
    CHECK(obj.status == 200);
  };
  WSAPIParserTester<value_type>::dispatch(helper, message, 65536, 1);
}
