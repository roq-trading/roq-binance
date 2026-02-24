/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/api.hpp"

#include "roq/logging.hpp"

#include "roq/exceptions.hpp"

#include "roq/utils/enum.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === HELPERS ===

namespace {
auto parse_side_effect_type(auto &side_effect_type) {
  if (std::empty(side_effect_type)) {
    return json::SideEffectType::UNDEFINED;
  }
  return utils::parse_enum<json::SideEffectType>(side_effect_type);
}
}  // namespace

// === IMPLEMENTATION ===

API API::create(Settings const &settings) {
  auto side_effect_type = parse_side_effect_type(settings.oms.margin_side_effect_type);
  return {
      .market_data{
          .exchange_info = "/api/v3/exchangeInfo"sv,
          .depth = "/api/v3/depth"sv,
      },
      .sapi{
          .listen_key = "/sapi/v1/margin/listen-key"sv,
          .isolated_margin_user_data_stream = "/sapi/v1/userDataStream/isolated"sv,
          .cross_account = "/sapi/v1/margin/account"sv,
          .margin_user_data_stream = "/sapi/v1/userListenToken"sv,
          .margin_open_orders = "/sapi/v1/margin/openOrders"sv,
          .margin_my_trades = "/sapi/v1/margin/myTrades"sv,
          .margin_order = "/sapi/v1/margin/order"sv,
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
      .margin_side_effect_type = side_effect_type,
  };
}

}  // namespace binance
}  // namespace roq
