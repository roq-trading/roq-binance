/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/market_stream_parser.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_depth_update_simple", "[json_depth_update]") {
  auto message = R"({)"
                 R"("stream":"btcusdt@depth@100ms",)"
                 R"("data":{)"
                 R"("e":"depthUpdate",)"
                 R"("E":1660386669440,)"
                 R"("s":"BTCUSDT",)"
                 R"("U":22415664363,)"
                 R"("u":22415664413,)"
                 R"("b":[)"
                 R"(["24488.92000000","0.00000000"],)"
                 R"(["24488.85000000","0.00000000"],)"
                 R"(["24488.84000000","0.00000000"],)"
                 R"(["24488.47000000","0.03500000"],)"
                 R"(["24488.46000000","0.00000000"],)"
                 R"(["24488.45000000","0.04234000"],)"
                 R"(["24488.44000000","0.00816000"],)"
                 R"(["24488.04000000","0.01428000"],)"
                 R"(["24487.60000000","0.00560000"],)"
                 R"(["24487.28000000","0.01283000"],)"
                 R"(["24487.03000000","0.00000000"],)"
                 R"(["24484.68000000","0.00000000"],)"
                 R"(["24483.97000000","0.00000000"],)"
                 R"(["24483.59000000","0.17000000"],)"
                 R"(["24481.96000000","0.02000000"],)"
                 R"(["24480.92000000","0.00327000"],)"
                 R"(["24480.61000000","0.00000000"],)"
                 R"(["24480.31000000","0.00000000"],)"
                 R"(["24479.91000000","0.12000000"],)"
                 R"(["24478.36000000","0.00000000"],)"
                 R"(["24478.10000000","0.02500000"]],)"
                 R"("a":[)"
                 R"(["24489.35000000","0.02000000"],)"
                 R"(["24489.82000000","0.02000000"],)"
                 R"(["24489.87000000","0.00000000"],)"
                 R"(["24489.91000000","0.00506000"],)"
                 R"(["24489.92000000","0.00000000"],)"
                 R"(["24489.93000000","0.03146000"],)"
                 R"(["24490.80000000","0.01560000"],)"
                 R"(["24490.91000000","0.05586000"],)"
                 R"(["24491.34000000","0.00000000"],)"
                 R"(["24491.36000000","0.00000000"],)"
                 R"(["24491.74000000","0.00000000"],)"
                 R"(["24492.29000000","0.00000000"],)"
                 R"(["24492.70000000","0.16443000"],)"
                 R"(["24493.43000000","0.00000000"],)"
                 R"(["24493.53000000","0.00000000"],)"
                 R"(["24493.55000000","0.00000000"],)"
                 R"(["24493.56000000","0.00000000"],)"
                 R"(["24493.63000000","0.00000000"],)"
                 R"(["24493.75000000","0.00000000"],)"
                 R"(["24494.33000000","0.00000000"],)"
                 R"(["24494.57000000","0.00000000"],)"
                 R"(["24495.38000000","0.00428000"],)"
                 R"(["24496.89000000","0.00000000"],)"
                 R"(["24498.85000000","0.00000000"],)"
                 R"(["24502.26000000","0.23993000"],)"
                 R"(["24562.30000000","0.02040000"])"
                 R"(])"
                 R"(})"
                 R"(})";
  core::Buffer buffer_(65536);
  core::json::Buffer buffer(buffer_);
  struct Handler : public json::MarketStreamParser::Handler {
    size_t counter = 0;
    void operator()(Trace<json::Error> const &, [[maybe_unused]] int32_t id) override { FAIL(); }
    void operator()(Trace<json::Result> const &, [[maybe_unused]] int32_t id) override { FAIL(); }
    void operator()(Trace<json::AggTrade> const &) override { FAIL(); }
    void operator()(Trace<json::Trade> const &) override { FAIL(); }
    void operator()(Trace<json::MiniTicker> const &) override { FAIL(); }
    void operator()(Trace<json::BookTicker> const &) override { FAIL(); }
    void operator()(Trace<json::Depth> const &, [[maybe_unused]] std::string_view const &symbol) override { FAIL(); }
    void operator()(Trace<json::DepthUpdate> const &) override { ++counter; }
  } handler;
  TraceInfo trace_info;
  json::MarketStreamParser::dispatch(handler, message, buffer, trace_info);
  CHECK(handler.counter == 1);
}
