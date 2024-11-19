/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/api.hpp"

#include "roq/exceptions.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === IMPLEMENTATION ===

API API::create(Settings const &) {
  return {
      .market_data{
          .exchange_info = "/api/v3/exchangeInfo"sv,
          .depth = "/api/v3/depth"sv,
      },
      .simple{
          .user_data_stream = "/api/v3/userDataStream"sv,
          .account = "/api/v3/account"sv,
          .open_orders = "/api/v3/openOrders"sv,
          .my_trades = "/api/v3/myTrades"sv,
          .order = "/api/v3/order"sv,
          .order_cancel_replace = "/api/v3/order/cancelReplace"sv,
      },
      .papi{
          .ping_path = "/papi/v1/time"sv,
          .listen_key = "/papi/v1/listenKey"sv,
          .balance = "/papi/v1/balance"sv,
          .margin_open_orders = "/papi/v1/margin/openOrders"sv,
          .margin_my_trades = "/papi/v1/margin/myTrades"sv,
          .margin_order = "/papi/v1/margin/order"sv,
          .margin_all_open_orders = "/papi/v1/margin/allOpenOrders"sv,
      },
  };
}

}  // namespace binance
}  // namespace roq
