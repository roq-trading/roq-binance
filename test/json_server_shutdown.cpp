/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "market_stream_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::ServerShutdown;

TEST_CASE("simple", "[json_server_shutdown]") {
  auto message = R"({)"
                 R"("stream":"!serverShutdown",)"
                 R"("data":{)"
                 R"("e":"serverShutdown",)"
                 R"("E":1770123456789)"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "!serverShutdown"sv);
    CHECK(obj.data.event_type == protocol::json::EventType::SERVER_SHUTDOWN);
    CHECK(obj.data.event_time == 1770123456789ms);
  };
  MarketStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
