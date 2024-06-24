/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/account.hpp"

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
  std::vector<std::byte> buffer(8192);
  json::Account account{message, buffer};
  CHECK(account.maker_commission == 10.0_a);
  CHECK(account.taker_commission == 10.0_a);
  CHECK(account.buyer_commission == 0.0_a);
  CHECK(account.seller_commission == 0.0_a);
  CHECK(account.can_trade == true);
  CHECK(account.can_withdraw == true);
  CHECK(account.can_deposit == true);
  CHECK(account.update_time == 1620467758218ms);
  CHECK(account.account_type == "MARGIN"sv);
  auto &balances = account.balances;
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
