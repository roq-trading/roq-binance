/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "market_stream_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::MiniTicker;

TEST_CASE("simple", "[json_mini_ticker]") {
  auto message = R"({)"
                 R"("stream":"btcusdt@miniTicker",)"
                 R"("data":{)"
                 R"("e":"24hrMiniTicker",)"
                 R"("E":1768627410022,)"
                 R"("s":"BTCUSDT",)"
                 R"("c":"95311.63000000",)"
                 R"("o":"95310.13000000",)"
                 R"("h":"95871.47000000",)"
                 R"("l":"94293.46000000",)"
                 R"("v":"9744.53927000",)"
                 R"("q":"928097362.08244870")"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "btcusdt@miniTicker"sv);
    CHECK(obj.data.event_type == protocol::json::EventType::_24HR_MINI_TICKER);
    CHECK(obj.data.event_time == 1768627410022ms);
    CHECK(obj.data.symbol == "BTCUSDT"sv);
  };
  MarketStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
