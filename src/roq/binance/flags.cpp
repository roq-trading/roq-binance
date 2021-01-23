/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/flags.h"

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

#include <string>

ABSL_FLAG(std::string, config_file, "", "config file (path)");

ABSL_FLAG(std::string, exchange, "binance", "exchange identifier (string)");

// rest

ABSL_FLAG(
    std::string,
    rest_uri,
    "https://testnet.binance.com/api/v1",
    "REST end-point (URI)");

ABSL_FLAG(uint32_t, rest_ping_freq_secs, 5, "ping frequency (seconds)");

ABSL_FLAG(
    std::string,
    rest_ping_path,
    "/api/v3/time",
    "URI path used for REST connection keep-alive messages");

ABSL_FLAG(uint32_t, rest_request_queue_depth, 5, "request: max queue depth");

ABSL_FLAG(
    uint32_t, rest_request_timeout_secs, 30, "request: timeout (seconds)");

ABSL_FLAG(
    uint32_t,
    rest_rate_limit_interval_secs,
    60,
    "rate limit: monitor interval (seconds)");

ABSL_FLAG(
    uint32_t,
    rest_rate_limit_max_requests,
    1200,
    "rate limit: max requests (per interval)");

// DEFINE_uint32_t, rest_depth_limit, 100, "depth limit (levels)");

ABSL_FLAG(
    uint32_t,
    rest_listen_key_refresh_secs,
    1800,
    "listen key refresh period (seconds)");

ABSL_FLAG(
    uint32_t,
    rest_order_recv_window_msecs,
    5000,
    "receive window (milliseconds), please refer to Binance documentation!");

ABSL_FLAG(
    bool,
    rest_cancel_on_disconnect,
    false,
    "cancel orders on disconnect? (bool)");

// ws

ABSL_FLAG(
    std::string,
    ws_uri,
    "wss://testnet.binance.com/realtime",
    "WebSocket end-point (URI)");

ABSL_FLAG(uint32_t, ws_ping_freq_secs, 5, "ping frequency (seconds)");

ABSL_FLAG(
    uint32_t,
    ws_max_subscriptions_per_stream,
    1024,
    "max subscriptions per connection (count)");

ABSL_FLAG(uint32_t, ws_subscribe_depth_levels, 20, "depth levels (count)");

ABSL_FLAG(
    uint32_t,
    ws_subscribe_depth_freq_msecs,
    100,
    "depth update frequency (milliseconds)");

ABSL_FLAG(
    bool,
    ws_subscribe_trade_details,
    false,
    "report individual matches for trade summary? (bool)");

// XXX review

ABSL_FLAG(uint32_t, encode_buffer_size, 1048576, "encode buffer size");

ABSL_FLAG(uint32_t, decode_buffer_size, 10485760, "decode buffer size");

// external

ABSL_DECLARE_FLAG(std::string, name);
ABSL_DECLARE_FLAG(uint32_t, cache_mbp_max_depth);

namespace roq {
namespace binance {

std::string_view Flags::config_file() {
  static const std::string result = absl::GetFlag(FLAGS_config_file);
  return result;
}

std::string_view Flags::exchange() {
  static const std::string result = absl::GetFlag(FLAGS_exchange);
  return result;
}

std::string_view Flags::rest_uri() {
  static const std::string result = absl::GetFlag(FLAGS_rest_uri);
  return result;
}

uint32_t Flags::rest_ping_freq_secs() {
  static const uint32_t result = absl::GetFlag(FLAGS_rest_ping_freq_secs);
  return result;
}

std::string_view Flags::rest_ping_path() {
  static const std::string result = absl::GetFlag(FLAGS_rest_ping_path);
  return result;
}

uint32_t Flags::rest_request_queue_depth() {
  static const uint32_t result = absl::GetFlag(FLAGS_rest_request_queue_depth);
  return result;
}

uint32_t Flags::rest_request_timeout_secs() {
  static const uint32_t result = absl::GetFlag(FLAGS_rest_request_timeout_secs);
  return result;
}

uint32_t Flags::rest_rate_limit_interval_secs() {
  static const uint32_t result =
      absl::GetFlag(FLAGS_rest_rate_limit_interval_secs);
  return result;
}

uint32_t Flags::rest_rate_limit_max_requests() {
  static const uint32_t result =
      absl::GetFlag(FLAGS_rest_rate_limit_max_requests);
  return result;
}

uint32_t Flags::rest_listen_key_refresh_secs() {
  static const uint32_t result =
      absl::GetFlag(FLAGS_rest_listen_key_refresh_secs);
  return result;
}

uint32_t Flags::rest_order_recv_window_msecs() {
  static const uint32_t result =
      absl::GetFlag(FLAGS_rest_order_recv_window_msecs);
  return result;
}

bool Flags::rest_cancel_on_disconnect() {
  static const bool result = absl::GetFlag(FLAGS_rest_cancel_on_disconnect);
  return result;
}

std::string_view Flags::ws_uri() {
  static const std::string result = absl::GetFlag(FLAGS_ws_uri);
  return result;
}

uint32_t Flags::ws_ping_freq_secs() {
  static const uint32_t result = absl::GetFlag(FLAGS_ws_ping_freq_secs);
  return result;
}

uint32_t Flags::ws_max_subscriptions_per_stream() {
  static const uint32_t result =
      absl::GetFlag(FLAGS_ws_max_subscriptions_per_stream);
  return result;
}

uint32_t Flags::ws_subscribe_depth_levels() {
  static const uint32_t result = absl::GetFlag(FLAGS_ws_subscribe_depth_levels);
  return result;
}

uint32_t Flags::ws_subscribe_depth_freq_msecs() {
  static const uint32_t result =
      absl::GetFlag(FLAGS_ws_subscribe_depth_freq_msecs);
  return result;
}

bool Flags::ws_subscribe_trade_details() {
  static const bool result = absl::GetFlag(FLAGS_ws_subscribe_trade_details);
  return result;
}

uint32_t Flags::encode_buffer_size() {
  static const uint32_t result = absl::GetFlag(FLAGS_encode_buffer_size);
  return result;
}

uint32_t Flags::decode_buffer_size() {
  static const uint32_t result = absl::GetFlag(FLAGS_decode_buffer_size);
  return result;
}

std::string_view Flags::name() {
  static const std::string result = absl::GetFlag(FLAGS_name);
  return result;
}

uint32_t Flags::cache_mbp_max_depth() {
  static const uint32_t result = absl::GetFlag(FLAGS_cache_mbp_max_depth);
  return result;
}

}  // namespace binance
}  // namespace roq
