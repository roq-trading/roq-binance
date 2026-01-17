/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "wsapi_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::WSAPICancelOrder;

TEST_CASE("empty", "[json_wsapi_cancel_order]") {
  auto message = R"({)"
                 R"("id":"iIQeAAkCYCgVtUwAAAACAAAAAAAAAAAAAAAA",)"
                 R"("status":200,)"
                 R"("result":{)"
                 R"("symbol":"BTCUSDT",)"
                 R"("origClientOrderId":"4AACYCgVtUwAAQAAAAAA",)"
                 R"("orderId":55471195855,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"4wACYCgVtUwAAgAAAAAA",)"
                 R"("transactTime":1768635168620,)"
                 R"("price":"80000.00000000",)"
                 R"("origQty":"0.00010000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("origQuoteOrderQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"CANCELED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("selfTradePreventionMode":"EXPIRE_MAKER"},)"
                 R"("rateLimits":[{)"
                 R"("rateLimitType":"REQUEST_WEIGHT",)"
                 R"("interval":"MINUTE",)"
                 R"("intervalNum":1,)"
                 R"("limit":6000,)"
                 R"("count":548)"
                 R"(})"
                 R"(])"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.id == "iIQeAAkCYCgVtUwAAAACAAAAAAAAAAAAAAAA"sv);
    CHECK(obj.status == 200);
  };
  WSAPIParserTester<value_type>::dispatch(helper, message, 65536, 1);
}
