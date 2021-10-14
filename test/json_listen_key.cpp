/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/listen_key.h"

using namespace roq;
using namespace roq::binance;

TEST(json_ticker, simple) {
  auto message = R"({)"
                 R"("listenKey":"F25mdh1CpogWkUK0A2F99A7hFTciD9fGiosfYoSqw8snTMDFrRt2eNWrBaI2")"
                 R"(})";
  auto obj = core::json::Parser::create<json::ListenKey>(message);
  EXPECT_EQ(obj.listen_key, "F25mdh1CpogWkUK0A2F99A7hFTciD9fGiosfYoSqw8snTMDFrRt2eNWrBaI2"_sv);
}
