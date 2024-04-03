/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/binance/settings.hpp"

namespace roq {
namespace binance {

struct API final {
  struct {
    std::string_view ping_path;
    std::string_view get_listen_key;
    std::string_view get_account;
    std::string_view get_open_orders;
    std::string_view get_trades;
    std::string_view order;
    std::string_view all_open_orders;
  } papi;

  // factory
  static API create(Settings const &);
};

}  // namespace binance
}  // namespace roq
