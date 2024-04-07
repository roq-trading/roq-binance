/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/binance/settings.hpp"

namespace roq {
namespace binance {

struct API final {
  struct {
    std::string_view exchange_info;
    std::string_view depth;
  } market_data;
  struct {
    std::string_view user_data_stream;
    std::string_view account;
    std::string_view open_orders;
    std::string_view my_trades;
    std::string_view order;
    std::string_view order_cancel_replace;
  } simple;
  struct {
    std::string_view ping_path;
    std::string_view listen_key;
    std::string_view balance;
    std::string_view margin_open_orders;
    std::string_view margin_my_trades;
    std::string_view margin_order;
    std::string_view margin_all_open_orders;
  } papi;

  // factory
  static API create(Settings const &);
};

}  // namespace binance
}  // namespace roq
