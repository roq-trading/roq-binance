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
          .get_listen_key = "/papi/v1/listenKey"sv,
          .get_account = "/papi/v1/account"sv,
          .get_open_orders = "/papi/v1/margin/openOrders"sv,
          .get_trades = "/papi/v1/margin/myTrades"sv,
          .order = "/papi/v1/margin/order"sv,
          .all_open_orders = "/papi/v1/margin/allOpenOrders"sv,
      },
  };
}

}  // namespace binance
}  // namespace roq
