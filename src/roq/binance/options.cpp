/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/options.h"

#include <absl/flags/flag.h>

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
