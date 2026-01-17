/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "market_stream_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::Result;

TEST_CASE("empty", "[json_result]") {
  auto message = R"({)"
                 R"("result":null,)"
                 R"("id":5000001)"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(std::size(obj.result) == 0);
    CHECK(obj.id == 5000001);
  };
  MarketStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
