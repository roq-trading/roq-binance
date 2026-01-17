/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "wsapi_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::WSAPISessionLogon;

TEST_CASE("empty", "[json_wsapi_session_logon]") {
  auto message = R"({)"
                 R"("id":"gYQeAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",)"
                 R"("status":200,)"
                 R"("result":{)"
                 R"("apiKey":"Fv7GPxz5gsAeDZ6ELyHlHvBaEaUB8PiqdFohT7ueOHlZ0Cbk5chgLFUIv6tVarnG",)"
                 R"("authorizedSince":1768633854263,)"
                 R"("connectedSince":1768633854138,)"
                 R"("returnRateLimits":true,)"
                 R"("serverTime":1768633854370,)"
                 R"("userDataStream":false},)"
                 R"("rateLimits":[{)"
                 R"("rateLimitType":"REQUEST_WEIGHT",)"
                 R"("interval":"MINUTE",)"
                 R"("intervalNum":1,)"
                 R"("limit":6000,)"
                 R"("count":25)"
                 R"(})"
                 R"(])"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.id == "gYQeAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"sv);
    CHECK(obj.status == 200);
  };
  WSAPIParserTester<value_type>::dispatch(helper, message, 65536, 1);
}
