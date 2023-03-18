/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/ws_api_parser.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;

using namespace Catch::literals;

TEST_CASE("simple", "[json_ws_listen_key]") {
  constexpr auto const message = R"({)"
                                 R"("id":"d3df8a61-98ea-4fe0-8f4e-0fcea5d418b0",)"
                                 R"("status":200,)"
                                 R"("result":{)"
                                 R"("listenKey":"eSWDvurLiumxeTwtGdHaLBozyJ9qzS9QcwOk3jmERrfqtf63IoQKwhD4CALz")"
                                 R"(},)"
                                 R"("rateLimits":[{)"
                                 R"("rateLimitType":"REQUEST_WEIGHT",)"
                                 R"("interval":"MINUTE",)"
                                 R"("intervalNum":1,)"
                                 R"("limit":1200,)"
                                 R"("count":12)"
                                 R"(})"
                                 R"(])"
                                 R"(})";
  struct Handler final : public json::WSAPIParser::Handler {
    void operator()(Trace<json::ListenKey> const &event) override {
      ++counter;
      auto &[trace_info, listen_key] = event;
      CHECK(listen_key.listen_key == "eSWDvurLiumxeTwtGdHaLBozyJ9qzS9QcwOk3jmERrfqtf63IoQKwhD4CALz"sv);
    }
    size_t counter = {};
  } handler;
  core::Buffer buffer_(4096);
  core::json::Buffer buffer(buffer_);
  TraceInfo trace_info;
  auto result = json::WSAPIParser::dispatch(handler, message, buffer, trace_info);
  CHECK(result == true);
  CHECK(handler.counter == 1);
}
