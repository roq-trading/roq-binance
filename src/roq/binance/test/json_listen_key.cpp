/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/listen_key.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;

using namespace Catch::literals;

TEST_CASE("json_ticker_simple", "[json_ticker]") {
  auto message = R"({)"
                 R"("listenKey":"F25mdh1CpogWkUK0A2F99A7hFTciD9fGiosfYoSqw8snTMDFrRt2eNWrBaI2")"
                 R"(})";
  auto obj = core::json::Parser::create<json::ListenKey>(message);
  CHECK(obj.listen_key == "F25mdh1CpogWkUK0A2F99A7hFTciD9fGiosfYoSqw8snTMDFrRt2eNWrBaI2"sv);
}
