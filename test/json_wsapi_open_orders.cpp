/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "wsapi_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::WSAPIOpenOrders;

TEST_CASE("empty", "[json_wsapi_open_orders]") {
  auto message = R"({)"
                 R"("id":"hIQeAAUAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",)"
                 R"("status":200,)"
                 R"("result":[],)"
                 R"("rateLimits":[{)"
                 R"("rateLimitType":"REQUEST_WEIGHT",)"
                 R"("interval":"MINUTE",)"
                 R"("intervalNum":1,)"
                 R"("limit":6000,)"
                 R"("count":127)"
                 R"(})"
                 R"(])"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.id == "hIQeAAUAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"sv);
    CHECK(obj.status == 200);
  };
  WSAPIParserTester<value_type>::dispatch(helper, message, 65536, 1);
}
