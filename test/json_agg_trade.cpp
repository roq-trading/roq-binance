/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "market_stream_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::AggTrade;

TEST_CASE("simple", "[json_agg_trade]") {
  auto message = R"({)"
                 R"("stream":"btcusdt@aggTrade",)"
                 R"("data":{)"
                 R"("e":"aggTrade",)"
                 R"("E":1768627409826,)"
                 R"("s":"BTCUSDT",)"
                 R"("a":3814332932,)"
                 R"("p":"95311.64000000",)"
                 R"("q":"0.00084000",)"
                 R"("f":5782154285,)"
                 R"("l":5782154285,)"
                 R"("T":1768627409825,)"
                 R"("m":false,)"
                 R"("M":true)"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "btcusdt@aggTrade"sv);
    CHECK(obj.data.event_type == protocol::json::EventType::AGG_TRADE);
    CHECK(obj.data.event_time == 1768627409826ms);
    CHECK(obj.data.symbol == "BTCUSDT"sv);
  };
  MarketStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
