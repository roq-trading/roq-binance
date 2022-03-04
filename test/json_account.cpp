/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <catch2/catch.hpp>

#include "roq/core/json/parser.h"

#include "roq/binance/json/account.h"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_account_simple", "[json_account]") {
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
  CHECK(obj.maker_commission == 10.0_a);
  CHECK(obj.taker_commission == 10.0_a);
  CHECK(obj.buyer_commission == 0.0_a);
  CHECK(obj.seller_commission == 0.0_a);
  CHECK(obj.can_trade == true);
  CHECK(obj.can_withdraw == true);
  CHECK(obj.can_deposit == true);
  CHECK(obj.update_time == 1620467758218ms);
  CHECK(obj.account_type == "MARGIN"sv);
  auto &balances = obj.balances;
  REQUIRE(std::size(balances) == 2);
  auto &b0 = balances[0];
  CHECK(b0.asset == "BTC"sv);
  CHECK(b0.free == 0.0_a);
  CHECK(b0.locked == 0.0_a);
  auto &b1 = balances[1];
  CHECK(b1.asset == "LTC"sv);
  CHECK(b1.free == 0.0_a);
  CHECK(b1.locked == 0.0_a);
}
