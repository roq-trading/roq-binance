/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

#include "roq/core/uri.h"

namespace roq {
namespace binance {

struct Flags final {
  static std::string_view config_file();
  static std::string_view exchange();
  static const core::URI &rest_uri();
  static std::chrono::seconds rest_ping_freq();
  static std::string_view rest_ping_path();
  static uint32_t rest_request_queue_depth();
  static std::chrono::seconds rest_request_timeout();
  static std::chrono::seconds rest_rate_limit_interval();
  static uint32_t rest_rate_limit_max_requests();
  static std::chrono::seconds rest_listen_key_refresh();
  static std::chrono::milliseconds rest_order_recv_window();
  static bool rest_cancel_on_disconnect();
  static const core::URI &ws_uri();
  static std::chrono::seconds ws_ping_freq();
  static uint32_t ws_max_subscriptions_per_stream();
  static uint32_t ws_subscribe_depth_levels();
  static std::chrono::milliseconds ws_subscribe_depth_freq();
  static bool ws_subscribe_trade_details();
  static uint32_t encode_buffer_size();
  static uint32_t decode_buffer_size();
  // external
  static std::string_view name();
  static uint32_t cache_mbp_max_depth();
};

}  // namespace binance
}  // namespace roq
