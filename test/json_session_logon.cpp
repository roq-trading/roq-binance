/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"

#include "roq/binance/json/wsapi_session_logon.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

// === IMPLEMENTATION ===

TEST_CASE("success", "[json_session_logon]") {
  auto const message = R"({)"
                       R"("id":"gYQeAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",)"
                       R"("status":200,)"
                       R"("result":{)"
                       R"("apiKey":"IUA8RM0EJObe4X7UWxy74jAp6s2TJL2flZgDoxzx5AU3ws1I81Gr8ID3MVwfnYVD",)"
                       R"("authorizedSince":1760498191681,)"
                       R"("connectedSince":1760498191561,)"
                       R"("returnRateLimits":true,)"
                       R"("serverTime":1760498191792,)"
                       R"("userDataStream":false)"
                       R"(},)"
                       R"("rateLimits":[{)"
                       R"("rateLimitType":"REQUEST_WEIGHT",)"
                       R"("interval":"MINUTE",)"
                       R"("intervalNum":1,)"
                       R"("limit":6000,)"
                       R"("count":24)"
                       R"(})"
                       R"(])"
                       R"(})";
  core::json::BufferStack buffers{8192, 1};
  json::WSAPISessionLogon obj{message, buffers};
}

TEST_CASE("failure", "[json_session_logon]") {
  auto const message = R"({)"
                       R"("id":"gYQeAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",)"
                       R"("status":400,)"
                       R"("error":{)"
                       R"("code":-1022,)"
                       R"("msg":"Signature for this request is not valid.")"
                       R"(},)"
                       R"("rateLimits":[{)"
                       R"("rateLimitType":"REQUEST_WEIGHT",)"
                       R"("interval":"MINUTE",)"
                       R"("intervalNum":1,)"
                       R"("limit":6000,)"
                       R"("count":24)"
                       R"(})"
                       R"(])"
                       R"(})";
  core::json::BufferStack buffers{8192, 1};
  json::WSAPISessionLogon obj{message, buffers};
}
