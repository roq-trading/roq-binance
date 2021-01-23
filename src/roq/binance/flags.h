/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <cstdint>
#include <string_view>

namespace roq {
namespace binance {

struct Flags final {
  static std::string_view config_file();
  static std::string_view exchange();
  static std::string_view rest_uri();
  static uint32_t rest_ping_freq_secs();
  static std::string_view rest_ping_path();
  static uint32_t rest_request_queue_depth();
  static uint32_t rest_request_timeout_secs();
  static uint32_t rest_rate_limit_interval_secs();
  static uint32_t rest_rate_limit_max_requests();
  static uint32_t rest_listen_key_refresh_secs();
  static uint32_t rest_order_recv_window_msecs();
  static bool rest_cancel_on_disconnect();
  static std::string_view ws_uri();
  static uint32_t ws_ping_freq_secs();
  static uint32_t ws_max_subscriptions_per_stream();
  static uint32_t ws_subscribe_depth_levels();
  static uint32_t ws_subscribe_depth_freq_msecs();
  static bool ws_subscribe_trade_details();
  static uint32_t encode_buffer_size();
  static uint32_t decode_buffer_size();
  // external
  static std::string_view name();
  static uint32_t cache_mbp_max_depth();
};

}  // namespace binance
}  // namespace roq
