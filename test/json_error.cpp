/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "market_stream_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::Error;

TEST_CASE("simple", "[json_error]") {
  auto message = R"({)"
                 R"("error":{)"
                 R"("code":2,)"
                 R"("msg":"Invalid request: streams must be an array")"
                 R"(},)"
                 R"("id":5000001)"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.error.code == 2);
    CHECK(obj.error.msg == "Invalid request: streams must be an array"sv);
    CHECK(obj.id == 5000001);
  };
  MarketStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
