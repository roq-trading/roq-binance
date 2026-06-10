/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "wsapi_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::WSAPIExecutionReport;

TEST_CASE("empty", "[json_wsapi_execution_report]") {
  auto message = R"({)"
                 R"("subscriptionId":0,)"
                 R"("event":{)"
                 R"("e":"executionReport",)"
                 R"("E":1768640962931,)"
                 R"("s":"BTCUSDT",)"
                 R"("c":"1wAChGqJuEwAAQAAAAAA",)"
                 R"("S":"BUY",)"
                 R"("o":"LIMIT",)"
                 R"("f":"GTC",)"
                 R"("q":"0.00010000",)"
                 R"("p":"80000.00000000",)"
                 R"("P":"0.00000000",)"
                 R"("F":"0.00000000",)"
                 R"("g":-1,)"
                 R"("C":"",)"
                 R"("x":"NEW",)"
                 R"("X":"NEW",)"
                 R"("r":"NONE",)"
                 R"("i":55472337221,)"
                 R"("l":"0.00000000",)"
                 R"("z":"0.00000000",)"
                 R"("L":"0.00000000",)"
                 R"("n":"0",)"
                 R"("N":null,)"
                 R"("T":1768640962931,)"
                 R"("t":-1,)"
                 R"("I":117599651999,)"
                 R"("w":true,)"
                 R"("m":false,)"
                 R"("M":false,)"
                 R"("O":1768640962931,)"
                 R"("Z":"0.00000000",)"
                 R"("Y":"0.00000000",)"
                 R"("Q":"0.00000000",)"
                 R"("W":1768640962931,)"
                 R"("V":"EXPIRE_MAKER")"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.subscription_id == 0);
    CHECK(obj.event.event_type == protocol::json::EventType::EXECUTION_REPORT);
  };
  WSAPIParserTester<value_type>::dispatch(helper, message, 65536, 1);
}
