/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"

#include "roq/binance/json/listen_key_ack.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;

using namespace Catch::literals;

using value_type = json::ListenKeyAck;

TEST_CASE("simple", "[json_listen_key_ack]") {
  auto message = R"({)"
                 R"("listenKey":"F25mdh1CpogWkUK0A2F99A7hFTciD9fGiosfYoSqw8snTMDFrRt2eNWrBaI2")"
                 R"(})"sv;
  auto helper = [&](value_type &obj) { CHECK(obj.listen_key == "F25mdh1CpogWkUK0A2F99A7hFTciD9fGiosfYoSqw8snTMDFrRt2eNWrBaI2"sv); };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}
