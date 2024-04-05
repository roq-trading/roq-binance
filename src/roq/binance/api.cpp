/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/api.hpp"

#include "roq/exceptions.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === IMPLEMENTATION ===

API API::create(Settings const &) {
  return {
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
