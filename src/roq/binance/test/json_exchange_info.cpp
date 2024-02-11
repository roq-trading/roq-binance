/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/binance/json/exchange_info.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_exchange_info_simple", "[json_exchange_info]") {
  auto message =
      R"({)"
      R"("timezone":"UTC",)"
      R"("serverTime":1634180185607,)"
      R"("rateLimits":[{)"
      R"("rateLimitType":"REQUEST_WEIGHT","interval":"MINUTE","intervalNum":1,"limit":1200)"
      R"(},{)"
      R"("rateLimitType":"ORDERS","interval":"SECOND","intervalNum":10,"limit":50)"
      R"(},{)"
      R"("rateLimitType":"ORDERS","interval":"DAY","intervalNum":1,"limit":160000)"
      R"(},{)"
      R"("rateLimitType":"RAW_REQUESTS","interval":"MINUTE","intervalNum":5,"limit":6100)"
      R"(})"
      R"(],)"
      R"("exchangeFilters":[],)"
      R"("symbols":[{)"
      R"("symbol":"ETHBTC",)"
      R"("status":"TRADING",)"
      R"("baseAsset":"ETH",)"
      R"("baseAssetPrecision":8,)"
      R"("quoteAsset":"BTC",)"
      R"("quotePrecision":8,)"
      R"("quoteAssetPrecision":8,)"
      R"("baseCommissionPrecision":8,)"
      R"("quoteCommissionPrecision":8,)"
      R"("orderTypes":["LIMIT","LIMIT_MAKER","MARKET","STOP_LOSS_LIMIT","TAKE_PROFIT_LIMIT"],)"
      R"("icebergAllowed":true,)"
      R"("ocoAllowed":true,)"
      R"("quoteOrderQtyMarketAllowed":true,)"
      R"("isSpotTradingAllowed":true,)"
      R"("isMarginTradingAllowed":true,)"
      R"("filters":[{)"
      R"("filterType":"PRICE_FILTER","minPrice":"0.00000100","maxPrice":"922327.00000000","tickSize":"0.00000100")"
      R"(},{)"
      R"("filterType":"PERCENT_PRICE","multiplierUp":"5","multiplierDown":"0.2","avgPriceMins":5)"
      R"(},{)"
      R"("filterType":"LOT_SIZE","minQty":"0.00010000","maxQty":"100000.00000000","stepSize":"0.00010000")"
      R"(},{)"
      R"("filterType":"MIN_NOTIONAL","minNotional":"0.00010000","applyToMarket":true,"avgPriceMins":5)"
      R"(},{)"
      R"("filterType":"ICEBERG_PARTS","limit":10)"
      R"(},{)"
      R"("filterType":"MARKET_LOT_SIZE","minQty":"0.00000000","maxQty":"825.12846187","stepSize":"0.00000000")"
      R"(},{)"
      R"("filterType":"MAX_NUM_ORDERS","maxNumOrders":200)"
      R"(},{)"
      R"("filterType":"MAX_NUM_ALGO_ORDERS","maxNumAlgoOrders":5)"
      R"(})"
      R"(],)"
      R"("permissions":["SPOT","MARGIN"])"
      R"(},{)"
      R"("symbol":"LTCBTC",)"
      R"("status":"TRADING",)"
      R"("baseAsset":"LTC",)"
      R"("baseAssetPrecision":8,)"
      R"("quoteAsset":"BTC",)"
      R"("quotePrecision":8,)"
      R"("quoteAssetPrecision":8,)"
      R"("baseCommissionPrecision":8,)"
      R"("quoteCommissionPrecision":8,)"
      R"("orderTypes":["LIMIT","LIMIT_MAKER","MARKET","STOP_LOSS_LIMIT","TAKE_PROFIT_LIMIT"],)"
      R"("icebergAllowed":true,)"
      R"("ocoAllowed":true,)"
      R"("quoteOrderQtyMarketAllowed":true,)"
      R"("isSpotTradingAllowed":true,)"
      R"("isMarginTradingAllowed":true,)"
      R"("filters":[{)"
      R"("filterType":"PRICE_FILTER","minPrice":"0.00000100","maxPrice":"100000.00000000","tickSize":"0.00000100")"
      R"(},{)"
      R"("filterType":"PERCENT_PRICE","multiplierUp":"5","multiplierDown":"0.2","avgPriceMins":5)"
      R"(},{)"
      R"("filterType":"LOT_SIZE","minQty":"0.00100000","maxQty":"100000.00000000","stepSize":"0.00100000")"
      R"(},{)"
      R"("filterType":"MIN_NOTIONAL","minNotional":"0.00010000","applyToMarket":true,"avgPriceMins":5)"
      R"(},{)"
      R"("filterType":"ICEBERG_PARTS","limit":10)"
      R"(},{)"
      R"("filterType":"MARKET_LOT_SIZE","minQty":"0.00000000","maxQty":"6397.92349617","stepSize":"0.00000000")"
      R"(},{)"
      R"("filterType":"MAX_NUM_ORDERS","maxNumOrders":200)"
      R"(},{)"
      R"("filterType":"MAX_NUM_ALGO_ORDERS","maxNumAlgoOrders":5)"
      R"(})"
      R"(],)"
      R"("permissions":["SPOT","MARGIN"])"
      R"(})"
      R"(])"
      R"(})";
  std::vector<std::byte> buffer(65536);
  json::ExchangeInfo obj{message, buffer};
  CHECK(obj.timezone == "UTC"sv);
  CHECK(obj.server_time == 1634180185607ms);
  // not parsed: rate_limits
  // not parsed: exchange_filters
  auto &symbols = obj.symbols;
  REQUIRE(std::size(obj.symbols) == 2);
  auto &s0 = symbols[0];
  CHECK(s0.symbol == "ETHBTC"sv);
  CHECK(s0.status == json::SymbolStatus::TRADING);
  CHECK(s0.base_asset == "ETH"sv);
  CHECK(s0.base_asset_precision == 8);
  CHECK(s0.quote_asset == "BTC"sv);
  CHECK(s0.quote_precision == 8);
  CHECK(s0.quote_asset_precision == 8);
  CHECK(s0.base_commission_precision == 8);
  CHECK(s0.quote_commission_precision == 8);
  // not parsed: order_type
  CHECK(s0.iceberg_allowed == true);
  CHECK(s0.oco_allowed == true);
  CHECK(s0.quote_order_qty_market_allowed == true);
  CHECK(s0.is_spot_trading_allowed == true);
  CHECK(s0.is_margin_trading_allowed == true);
  // not parsed: filters
  // not parsed: permissions
  auto &s1 = symbols[1];
  CHECK(s1.symbol == "LTCBTC"sv);
  CHECK(s1.status == json::SymbolStatus::TRADING);
  CHECK(s1.base_asset == "LTC"sv);
  CHECK(s1.base_asset_precision == 8);
  CHECK(s1.quote_asset == "BTC"sv);
  CHECK(s1.quote_precision == 8);
  CHECK(s1.quote_asset_precision == 8);
  CHECK(s1.base_commission_precision == 8);
  CHECK(s1.quote_commission_precision == 8);
  // not parsed: order_type
  CHECK(s1.iceberg_allowed == true);
  CHECK(s1.oco_allowed == true);
  CHECK(s1.quote_order_qty_market_allowed == true);
  CHECK(s1.is_spot_trading_allowed == true);
  CHECK(s1.is_margin_trading_allowed == true);
  // not parsed: filters
  // not parsed: permissions
}
