/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "wsapi_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::WSAPIEventStreamTerminated;

TEST_CASE("empty", "[json_wsapi_event_stream_terminated]") {
  auto message = R"({)"
                 R"("subscriptionId":0,)"
                 R"("event":{)"
                 R"("e":"eventStreamTerminated",)"
                 R"("E":1771931296343)"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.subscription_id == 0);
    CHECK(obj.event.event_time == 1771931296343ms);
  };
  WSAPIParserTester<value_type>::dispatch(helper, message, 65536, 1);
}
