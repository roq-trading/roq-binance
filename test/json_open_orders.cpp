/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/open_orders.h"

using namespace roq;
using namespace roq::binance;

using namespace std::chrono_literals;

TEST(json_open_orders, simple_empty) {
  auto message = R"([])";
  core::Buffer buffer_(8192);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::OpenOrders>(message, buffer);
  ASSERT_EQ(std::size(obj.data), 0);
}
