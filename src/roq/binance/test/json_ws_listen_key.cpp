/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/ws_api_parser.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;

using namespace Catch::literals;

TEST_CASE("simple", "[json_ws_listen_key]") {
  constexpr auto const message = R"({)"
                                 R"("id":"AQAAAAJ7Fc1bB7Fo3jpZd34L",)"
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
    void operator()(Trace<json::Error> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::ListenKey> const &event, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      ++counter;
      auto &[trace_info, listen_key] = event;
      CHECK(listen_key.listen_key == "eSWDvurLiumxeTwtGdHaLBozyJ9qzS9QcwOk3jmERrfqtf63IoQKwhD4CALz"sv);
    }
    void operator()(
        Trace<json::Account> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::OpenOrders> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(Trace<json::Trades> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::CancelAllOpenOrders> const &,
        json::WSAPIRequest const &,
        [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::NewOrder> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::CancelOrder> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::CancelReplaceOrder> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::CancelReplaceOrderError> const &,
        json::WSAPIRequest const &,
        [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    size_t counter = {};
  } handler;
  std::vector<std::byte> buffer(8192);
  TraceInfo trace_info;
  auto result = json::WSAPIParser::dispatch(handler, message, buffer, trace_info);
  CHECK(result == true);
  CHECK(handler.counter == 1);
}
