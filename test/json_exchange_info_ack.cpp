/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"

#include "roq/binance/protocol/json/exchange_info_ack.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::ExchangeInfoAck;

TEST_CASE("simple", "[json_exchange_info_ack]") {
  auto message = R"({)"
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
                 R"(})"sv;
  auto helper = [&](value_type &obj) {
    CHECK(obj.timezone == "UTC"sv);
    CHECK(obj.server_time == 1634180185607ms);
    // not parsed: rate_limits
    // not parsed: exchange_filters
    auto &symbols = obj.symbols;
    REQUIRE(std::size(obj.symbols) == 2);
    auto &symbol_0 = symbols[0];
    CHECK(symbol_0.symbol == "ETHBTC"sv);
    CHECK(symbol_0.status == protocol::json::SymbolStatus::TRADING);
    CHECK(symbol_0.base_asset == "ETH"sv);
    CHECK(symbol_0.base_asset_precision == 8);
    CHECK(symbol_0.quote_asset == "BTC"sv);
    CHECK(symbol_0.quote_precision == 8);
    CHECK(symbol_0.quote_asset_precision == 8);
    CHECK(symbol_0.base_commission_precision == 8);
    CHECK(symbol_0.quote_commission_precision == 8);
    // not parsed: order_type
    CHECK(symbol_0.iceberg_allowed == true);
    CHECK(symbol_0.oco_allowed == true);
    CHECK(symbol_0.quote_order_qty_market_allowed == true);
    CHECK(symbol_0.is_spot_trading_allowed == true);
    CHECK(symbol_0.is_margin_trading_allowed == true);
    // not parsed: filters
    // not parsed: permissions
    auto &symbol_1 = symbols[1];
    CHECK(symbol_1.symbol == "LTCBTC"sv);
    CHECK(symbol_1.status == protocol::json::SymbolStatus::TRADING);
    CHECK(symbol_1.base_asset == "LTC"sv);
    CHECK(symbol_1.base_asset_precision == 8);
    CHECK(symbol_1.quote_asset == "BTC"sv);
    CHECK(symbol_1.quote_precision == 8);
    CHECK(symbol_1.quote_asset_precision == 8);
    CHECK(symbol_1.base_commission_precision == 8);
    CHECK(symbol_1.quote_commission_precision == 8);
    // not parsed: order_type
    CHECK(symbol_1.iceberg_allowed == true);
    CHECK(symbol_1.oco_allowed == true);
    CHECK(symbol_1.quote_order_qty_market_allowed == true);
    CHECK(symbol_1.is_spot_trading_allowed == true);
    CHECK(symbol_1.is_margin_trading_allowed == true);
    // not parsed: filters
    // not parsed: permissions
  };
  core::json::BufferStack buffers{65536, 2};
  value_type obj{message, buffers};
  helper(obj);
}
