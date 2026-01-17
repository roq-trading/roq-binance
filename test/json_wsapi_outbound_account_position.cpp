/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "wsapi_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::WSAPIOutboundAccountPosition;

TEST_CASE("empty", "[json_wsapi_outbound_account_position]") {
  auto message = R"({)"
                 R"("subscriptionId":0,)"
                 R"("event":{)"
                 R"("e":"outboundAccountPosition",)"
                 R"("E":1768640962931,)"
                 R"("u":1768640962931,)"
                 R"("B":[{)"
                 R"("a":"BTC",)"
                 R"("f":"0.00012672",)"
                 R"("l":"0.00000000")"
                 R"(},{)"
                 R"("a":"BNB",)"
                 R"("f":"0.00000000",)"
                 R"("l":"0.00000000")"
                 R"(},{)"
                 R"("a":"USDT",)"
                 R"("f":"283.16729644",)"
                 R"("l":"8.00000000")"
                 R"(})"
                 R"(])"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.subscription_id == 0);
    CHECK(obj.event.event_type == json::EventType::OUTBOUND_ACCOUNT_POSITION);
  };
  WSAPIParserTester<value_type>::dispatch(helper, message, 65536, 1);
}
