/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"
#include "roq/core/json/parser.hpp"

#include "roq/binance/json/encoder.hpp"
#include "roq/binance/json/new_order.hpp"

using namespace roq;
using namespace roq::binance;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

namespace {
auto const CREATE_ORDER_TEMPLATE = json::CreateOrderTemplate{};
}

/*
TEST_CASE("json_new_order_simple", "[json_new_order]") {
  auto message = R"({)"
                 R"("symbol":"LTCBTC",)"
                 R"("orderId":778507063,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"qQAC6gMAAQAAS-jxw4MW",)"
                 R"("transactTime":1634214384058,)"
                 R"("price":"0.00304100",)"
                 R"("origQty":"0.10000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"NEW",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("fills":[])"
                 R"(})";
  core::json::BufferStack buffer{8192, 1};
  json::NewOrder obj{message, buffer};
  CHECK(obj.symbol == "LTCBTC"sv);
  CHECK(obj.order_id == 778507063);
  CHECK(obj.order_list_id == -1);
  CHECK(obj.client_order_id == "qQAC6gMAAQAAS-jxw4MW"sv);
  CHECK(obj.transact_time == 1634214384058ms);
  CHECK(obj.price == 0.003041_a);
  CHECK(obj.orig_qty == 0.1_a);
  CHECK(obj.cummulative_quote_qty == 0.0_a);
  CHECK(obj.status == json::OrderStatus::NEW);
  CHECK(obj.time_in_force == json::TimeInForce::GTC);
  CHECK(obj.type == json::OrderType::LIMIT);
  CHECK(obj.side == json::Side::BUY);
  CHECK(std::size(obj.fills) == 0);
}

TEST_CASE("json_new_order_simple_maker", "[json_new_order]") {
  auto message = R"({)"
                 R"("symbol":"LTCUSDT",)"
                 R"("orderId":2426862755,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"SwAC6wMAAQAA8foJ1iQX",)"
                 R"("transactTime":1634906177360,)"
                 R"("price":"198.30000000",)"
                 R"("origQty":"0.10000000",)"
                 R"("executedQty":"0.00000000",)"
                 R"("cummulativeQuoteQty":"0.00000000",)"
                 R"("status":"NEW",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"BUY",)"
                 R"("fills":[])"
                 R"(})";
  core::json::BufferStack buffer{8192, 1};
  json::NewOrder obj{message, buffer};
  CHECK(obj.symbol == "LTCUSDT"sv);
  CHECK(obj.order_id == 2426862755);
  CHECK(obj.order_list_id == -1);
  CHECK(obj.client_order_id == "SwAC6wMAAQAA8foJ1iQX"sv);
  CHECK(obj.transact_time == 1634906177360ms);
  CHECK(obj.price == 198.3_a);
  CHECK(obj.orig_qty == 0.1_a);
  CHECK(obj.cummulative_quote_qty == 0.0_a);
  CHECK(obj.status == json::OrderStatus::NEW);
  CHECK(obj.time_in_force == json::TimeInForce::GTC);
  CHECK(obj.type == json::OrderType::LIMIT);
  CHECK(obj.side == json::Side::BUY);
  auto &fills = obj.fills;
  CHECK(std::size(fills) == 0);
}

TEST_CASE("json_new_order_simple_taker", "[json_new_order]") {
  auto message = R"({)"
                 R"("symbol":"LTCUSDT",)"
                 R"("orderId":2426923399,)"
                 R"("orderListId":-1,)"
                 R"("clientOrderId":"bQAC6QMAAQAAaNElbSUX",)"
                 R"("transactTime":1634908712538,)"
                 R"("price":"198.50000000",)"
                 R"("origQty":"0.10000000",)"
                 R"("executedQty":"0.10000000",)"
                 R"("cummulativeQuoteQty":"19.85000000",)"
                 R"("status":"FILLED",)"
                 R"("timeInForce":"GTC",)"
                 R"("type":"LIMIT",)"
                 R"("side":"SELL",)"
                 R"("fills":[{)"
                 R"("price":"198.50000000",)"
                 R"("qty":"0.10000000",)"
                 R"("commission":"0.00003030",)"
                 R"("commissionAsset":"BNB",)"
                 R"("tradeId":207085107)"
                 R"(})"
                 R"(])"
                 R"(})";
  core::json::BufferStack buffer{8192, 1};
  json::NewOrder obj{message, buffer};
  CHECK(obj.symbol == "LTCUSDT"sv);
  CHECK(obj.order_id == 2426923399);
  CHECK(obj.order_list_id == -1);
  CHECK(obj.client_order_id == "bQAC6QMAAQAAaNElbSUX"sv);
  CHECK(obj.transact_time == 1634908712538ms);
  CHECK(obj.price == 198.5_a);
  CHECK(obj.orig_qty == 0.1_a);
  CHECK(obj.cummulative_quote_qty == 19.85_a);
  CHECK(obj.status == json::OrderStatus::FILLED);
  CHECK(obj.time_in_force == json::TimeInForce::GTC);
  CHECK(obj.type == json::OrderType::LIMIT);
  CHECK(obj.side == json::Side::SELL);
  auto &fills = obj.fills;
  CHECK(std::size(fills) == 1);
  auto &fill_0 = fills[0];
  CHECK(fill_0.price == 198.5_a);
  CHECK(fill_0.qty == 0.1_a);
  CHECK(fill_0.commission == 0.0000303_a);
  CHECK(fill_0.commission_asset == "BNB"sv);
  CHECK(fill_0.trade_id == 207085107);
}

TEST_CASE("json_new_order_create_market", "[json_new_order]") {
  auto create_order = CreateOrder{
      .account = {},
      .order_id = {},
      .exchange = {},
      .symbol = "BTC"sv,
      .side = Side::BUY,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = NaN,
      .order_type = OrderType::MARKET,
      .time_in_force = {},
      .execution_instructions = {},
      .request_template = {},
      .quantity = 123.4,
      .price = NaN,
      .stop_price = NaN,
      .leverage = NaN,
      .routing_id = {},
      .strategy_id = {},
  };
  server::oms::Order order = {};
  order.price_precision.precision = Precision::_2;
  order.quantity_precision.precision = Precision::_2;
  std::vector<char> buffer;
  auto body = json::Encoder::new_order(buffer, create_order, order, "abc123"sv, CREATE_ORDER_TEMPLATE, 5s);
  auto expected =
      "newClientOrderId=abc123&"
      "quantity=123.40&"
      "recvWindow=5000&"
      "side=BUY&"
      "symbol=BTC&"
      "type=MARKET"sv;
  CHECK(body == expected);
}

TEST_CASE("json_new_order_create_limit", "[json_new_order]") {
  auto create_order = CreateOrder{
      .account = {},
      .order_id = {},
      .exchange = {},
      .symbol = "BTC"sv,
      .side = Side::BUY,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = NaN,
      .order_type = OrderType::LIMIT,
      .time_in_force = TimeInForce::GTC,
      .execution_instructions = {},
      .request_template = {},
      .quantity = 123.4,
      .price = 123.4,
      .stop_price = NaN,
      .leverage = NaN,
      .routing_id = {},
      .strategy_id = {},
  };
  server::oms::Order order = {};
  order.price_precision.precision = Precision::_2;
  order.quantity_precision.precision = Precision::_2;
  std::vector<char> buffer;
  auto body = json::Encoder::new_order(buffer, create_order, order, "abc123"sv, CREATE_ORDER_TEMPLATE, 5s);
  auto expected =
      "newClientOrderId=abc123&"
      "price=123.40&"
      "quantity=123.40&"
      "recvWindow=5000&"
      "side=BUY&"
      "symbol=BTC&"
      "timeInForce=GTC&"
      "type=LIMIT"sv;
  CHECK(body == expected);
}

TEST_CASE("json_new_order_create_limit_maker", "[json_new_order]") {
  auto create_order = CreateOrder{
      .account = {},
      .order_id = {},
      .exchange = {},
      .symbol = "BTC"sv,
      .side = Side::BUY,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = NaN,
      .order_type = OrderType::LIMIT,
      .time_in_force = TimeInForce::GTC,
      .execution_instructions = {ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE},
      .request_template = {},
      .quantity = 123.4,
      .price = 123.4,
      .stop_price = NaN,
      .leverage = NaN,
      .routing_id = {},
      .strategy_id = {},
  };
  server::oms::Order order = {};
  order.price_precision.precision = Precision::_2;
  order.quantity_precision.precision = Precision::_2;
  std::vector<char> buffer;
  auto body = json::Encoder::new_order(buffer, create_order, order, "abc123"sv, CREATE_ORDER_TEMPLATE, 5s);
  auto expected =
      "newClientOrderId=abc123&"
      "price=123.40&"
      "quantity=123.40&"
      "recvWindow=5000&"
      "side=BUY&"
      "symbol=BTC&"
      "type=LIMIT_MAKER"sv;
  CHECK(body == expected);
}

TEST_CASE("json_new_order_create_stop_loss", "[json_new_order]") {
  auto create_order = CreateOrder{
      .account = {},
      .order_id = {},
      .exchange = {},
      .symbol = "BTC"sv,
      .side = Side::BUY,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = NaN,
      .order_type = OrderType::MARKET,
      .time_in_force = {},
      .execution_instructions = {},
      .request_template = {},
      .quantity = 123.4,
      .price = NaN,
      .stop_price = 123.4,
      .leverage = NaN,
      .routing_id = {},
      .strategy_id = {},
  };
  server::oms::Order order = {};
  order.price_precision.precision = Precision::_2;
  order.quantity_precision.precision = Precision::_2;
  std::vector<char> buffer;
  auto body = json::Encoder::new_order(buffer, create_order, order, "abc123"sv, CREATE_ORDER_TEMPLATE, 5s);
  auto expected =
      "newClientOrderId=abc123&"
      "quantity=123.40&"
      "recvWindow=5000&"
      "side=BUY&"
      "stopPrice=123.40&"
      "symbol=BTC&"
      "type=STOP_LOSS"sv;
  CHECK(body == expected);
}

TEST_CASE("json_new_order_create_stop_loss_limit", "[json_new_order]") {
  auto create_order = CreateOrder{
      .account = {},
      .order_id = {},
      .exchange = {},
      .symbol = "BTC"sv,
      .side = Side::BUY,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = NaN,
      .order_type = OrderType::LIMIT,
      .time_in_force = TimeInForce::GTC,
      .execution_instructions = {},
      .request_template = {},
      .quantity = 123.4,
      .price = 123.4,
      .stop_price = 123.4,
      .leverage = NaN,
      .routing_id = {},
      .strategy_id = {},
  };
  server::oms::Order order = {};
  order.price_precision.precision = Precision::_2;
  order.quantity_precision.precision = Precision::_2;
  std::vector<char> buffer;
  auto body = json::Encoder::new_order(buffer, create_order, order, "abc123"sv, CREATE_ORDER_TEMPLATE, 5s);
  auto expected =
      "newClientOrderId=abc123&"
      "price=123.40&"
      "quantity=123.40&"
      "recvWindow=5000&"
      "side=BUY&"
      "stopPrice=123.40&"
      "symbol=BTC&"
      "timeInForce=GTC&"
      "type=STOP_LOSS_LIMIT"sv;
  CHECK(body == expected);
}

TEST_CASE("json_new_order_margin", "[json_new_order]") {
  auto create_order = CreateOrder{
      .account = {},
      .order_id = {},
      .exchange = {},
      .symbol = "BTC"sv,
      .side = Side::BUY,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = NaN,
      .order_type = OrderType::LIMIT,
      .time_in_force = TimeInForce::GTC,
      .execution_instructions = {},
      .request_template = {},
      .quantity = 123.4,
      .price = 123.4,
      .stop_price = 123.4,
      .leverage = NaN,
      .routing_id = {},
      .strategy_id = {},
  };
  server::oms::Order order = {};
  order.price_precision.precision = Precision::_2;
  order.quantity_precision.precision = Precision::_2;
  std::vector<char> buffer;
  auto body = json::Encoder::new_order(buffer, create_order, order, "abc123"sv, CREATE_ORDER_TEMPLATE, 5s, json::SideEffectType::AUTO_BORROW_REPAY);
  auto expected =
      "newClientOrderId=abc123&"
      "price=123.40&"
      "quantity=123.40&"
      "recvWindow=5000&"
      "side=BUY&"
      "sideEffectType=AUTO_BORROW_REPAY&"
      "stopPrice=123.40&"
      "symbol=BTC&"
      "timeInForce=GTC&"
      "type=STOP_LOSS_LIMIT"sv;
  CHECK(body == expected);
}
*/
