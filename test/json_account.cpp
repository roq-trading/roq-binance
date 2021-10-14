/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/binance/json/account.h"

using namespace roq;
using namespace roq::binance;

using namespace std::chrono_literals;

TEST(json_account, simple) {
  // note! balances has been truncated
  auto message = R"({)"
                 R"("makerCommission":10,)"
                 R"("takerCommission":10,)"
                 R"("buyerCommission":0,)"
                 R"("sellerCommission":0,)"
                 R"("canTrade":true,)"
                 R"("canWithdraw":true,)"
                 R"("canDeposit":true,)"
                 R"("updateTime":1620467758218,)"
                 R"("accountType":"MARGIN",)"
                 R"("balances":[{)"
                 R"("asset":"BTC",)"
                 R"("free":"0.00000000",)"
                 R"("locked":"0.00000000")"
                 R"(},{)"
                 R"("asset":"LTC",)"
                 R"("free":"0.00000000",)"
                 R"("locked":"0.00000000")"
                 R"(})"
                 R"(])"
                 R"(})";
  core::Buffer buffer_(8192);
  core::json::Buffer buffer(buffer_);
  auto obj = core::json::Parser::create<json::Account>(message, buffer);
  EXPECT_DOUBLE_EQ(obj.maker_commission, 10.0);
  EXPECT_DOUBLE_EQ(obj.taker_commission, 10.0);
  EXPECT_DOUBLE_EQ(obj.buyer_commission, 0.0);
  EXPECT_DOUBLE_EQ(obj.seller_commission, 0.0);
  EXPECT_EQ(obj.can_trade, true);
  EXPECT_EQ(obj.can_withdraw, true);
  EXPECT_EQ(obj.can_deposit, true);
  EXPECT_EQ(obj.update_time, 1620467758218ms);
  EXPECT_EQ(obj.account_type, "MARGIN"_sv);
  auto &balances = obj.balances;
  ASSERT_EQ(std::size(balances), 2);
  auto &b0 = balances[0];
  EXPECT_EQ(b0.asset, "BTC"_sv);
  EXPECT_DOUBLE_EQ(b0.free, 0.0);
  EXPECT_DOUBLE_EQ(b0.locked, 0.0);
  auto &b1 = balances[1];
  EXPECT_EQ(b1.asset, "LTC"_sv);
  EXPECT_DOUBLE_EQ(b1.free, 0.0);
  EXPECT_DOUBLE_EQ(b1.locked, 0.0);
}
