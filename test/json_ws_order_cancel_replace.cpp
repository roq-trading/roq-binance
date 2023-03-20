/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/ws_api_parser.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;

using namespace Catch::literals;

TEST_CASE("simple", "[json_ws_order_cancel_replace]") {
  constexpr auto const message = R"({)"
                                 R"("id":"UEIPAAkC7gMAAAIAAADvAwAA",)"
                                 R"("status":200,)"
                                 R"("result":{)"
                                 R"("cancelResult":"SUCCESS",)"
                                 R"("newOrderResult":"SUCCESS",)"
                                 R"("cancelResponse":{)"
                                 R"("symbol":"BTCUSDT",)"
                                 R"("origClientOrderId":"RwAC7gMAAQAADJEBxYg_",)"
                                 R"("orderId":20443186753,)"
                                 R"("orderListId":-1,)"
                                 R"("clientOrderId":"4gAC7gMAAgAA4-ozxYg_",)"
                                 R"("price":"28219.61000000",)"
                                 R"("origQty":"0.00050000",)"
                                 R"("executedQty":"0.00000000",)"
                                 R"("cummulativeQuoteQty":"0.00000000",)"
                                 R"("status":"CANCELED",)"
                                 R"("timeInForce":"GTC",)"
                                 R"("type":"LIMIT",)"
                                 R"("side":"BUY",)"
                                 R"("selfTradePreventionMode":"NONE")"
                                 R"(},)"
                                 R"("newOrderResponse":{)"
                                 R"("symbol":"BTCUSDT",)"
                                 R"("orderId":20443189559,)"
                                 R"("orderListId":-1,)"
                                 R"("clientOrderId":"qQAC7wMAAQAAq-szxYg_",)"
                                 R"("transactTime":1679315856746,)"
                                 R"("price":"28227.68000000",)"
                                 R"("origQty":"0.00050000",)"
                                 R"("executedQty":"0.00000000",)"
                                 R"("cummulativeQuoteQty":"0.00000000",)"
                                 R"("status":"NEW",)"
                                 R"("timeInForce":"GTC",)"
                                 R"("type":"LIMIT",)"
                                 R"("side":"BUY",)"
                                 R"("workingTime":1679315856746,)"
                                 R"("fills":[],)"
                                 R"("selfTradePreventionMode":"NONE")"
                                 R"(})"
                                 R"(},)"
                                 R"("rateLimits":[{)"
                                 R"("rateLimitType":"ORDERS",)"
                                 R"("interval":"SECOND",)"
                                 R"("intervalNum":10,)"
                                 R"("limit":50,)"
                                 R"("count":2)"
                                 R"(},{)"
                                 R"("rateLimitType":"ORDERS",)"
                                 R"("interval":"DAY",)"
                                 R"("intervalNum":1,)"
                                 R"("limit":160000,)"
                                 R"("count":19)"
                                 R"(},{)"
                                 R"("rateLimitType":"REQUEST_WEIGHT",)"
                                 R"("interval":"MINUTE",)"
                                 R"("intervalNum":1,)"
                                 R"("limit":1200,)"
                                 R"("count":18)"
                                 R"(})"
                                 R"(])"
                                 R"(})"sv;
  struct Handler final : public json::WSAPIParser::Handler {
    void operator()(Trace<json::Error> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::ListenKey> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::Account> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
      FAIL();
    }
    void operator()(
        Trace<json::OpenOrders> const &, json::WSAPIRequest const &, [[maybe_unused]] int32_t status) override {
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
        Trace<json::CancelReplaceOrder> const &event,
        json::WSAPIRequest const &,
        [[maybe_unused]] int32_t status) override {
      ++counter;
      auto &[trace_info, cancel_replace_order] = event;
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
