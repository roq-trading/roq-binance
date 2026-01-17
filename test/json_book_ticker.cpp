/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "market_stream_parser_tester.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::BookTicker;

TEST_CASE("simple", "[json_book_ticker]") {
  auto message = R"({)"
                 R"("stream":"ethusdt@bookTicker",)"
                 R"("data":{)"
                 R"("u":68791858400,)"
                 R"("s":"ETHUSDT",)"
                 R"("b":"3290.28000000",)"
                 R"("B":"65.97110000",)"
                 R"("a":"3290.29000000",)"
                 R"("A":"3.02760000")"
                 R"(})"
                 R"(})"sv;
  auto helper = [](value_type const &obj) {
    CHECK(obj.stream == "ethusdt@bookTicker"sv);
    CHECK(obj.data.order_book_update_id == 68791858400);
    CHECK(obj.data.symbol == "ETHUSDT"sv);
  };
  MarketStreamParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
