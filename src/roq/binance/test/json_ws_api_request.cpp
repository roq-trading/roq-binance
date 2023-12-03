/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/binance/json/ws_api_request.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

// === IMPLEMENTATION ===

TEST_CASE("simple", "[json_ws_api_request]") {
  auto request = json::WSAPIRequest{
      .sequence = 1,
      .type = json::WSAPIType::LISTEN_KEY_CREATE,
      .user_id = 123,
      .order_id = 123456789,
      .version = 987654321,
      .order_id_2 = 192837465,
  };
  std::vector<char> buffer;
  auto message = json::WSAPIRequest::encode(buffer, request);
  CHECK(message == "AQAAAAJ7Fc1bB7Fo3jpZd34L"sv);
  auto result = json::WSAPIRequest::decode(message);
  CHECK(result.sequence == request.sequence);
  CHECK(result.type == request.type);
  CHECK(result.user_id == request.user_id);
  CHECK(result.order_id == request.order_id);
  CHECK(result.version == request.version);
  CHECK(result.order_id_2 == request.order_id_2);
}
