/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "wsapi_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::WSAPIUserDataStreamSubscribe;

TEST_CASE("empty", "[json_wsapi_user_data_stream_subscribe]") {
  auto message = R"({)"
                 R"("id":"goQeAAMAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",)"
                 R"("status":200,)"
                 R"("result":{)"
                 R"("subscriptionId":0)"
                 R"(},)"
                 R"("rateLimits":[{)"
                 R"("rateLimitType":"REQUEST_WEIGHT",)"
                 R"("interval":"MINUTE",)"
                 R"("intervalNum":1,)"
                 R"("limit":6000,)"
                 R"("count":27)"
                 R"(})"
                 R"(])"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.id == "goQeAAMAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"sv);
    CHECK(obj.status == 200);
  };
  WSAPIParserTester<value_type>::dispatch(helper, message, 65536, 1);
}
