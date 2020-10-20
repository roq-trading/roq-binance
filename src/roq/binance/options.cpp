/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/options.h"

DEFINE_string(config_file, "", "config file (path)");

DEFINE_string(exchange, "binance", "exchange identifier (string)");

DEFINE_uint32(download_timeout_secs, 15, "download time-out (seconds)");

DEFINE_string(
    rest_uri, "https://testnet.binance.com/api/v1", "REST end-point (URI)");

DEFINE_uint32(rest_ping_freq_secs, 5, "ping frequency (seconds)");

DEFINE_string(
    rest_ping_path,
    "/api/v3/time",
    "URI path used for REST connection keep-alive messages");

DEFINE_uint32(
    rest_rate_limit_interval_secs,
    60,
    "rate limit: monitor interval (seconds)");

DEFINE_uint32(
    rest_rate_limit_max_requests,
    1200,
    "rate limit: max requests (per interval)");

DEFINE_uint32(rest_depth_limit, 100, "depth limit (levels)");

DEFINE_uint32(
    rest_listen_key_refresh_secs, 1800, "listen key refresh period (seconds)");

DEFINE_uint32(rest_recv_window_secs, 5000, "receive window (seconds)");

DEFINE_string(
    ws_uri, "wss://testnet.binance.com/realtime", "WebSocket end-point (URI)");

DEFINE_uint32(ws_ping_freq_secs, 5, "ping frequency (seconds)");

DEFINE_uint32(ws_depth_levels, 20, "depth levels (count)");

DEFINE_uint32(
    ws_depth_freq_msecs, 100, "depth update frequency (milliseconds)");

DEFINE_bool(
    ws_trade_details,
    false,
    "report individual matches for trade summary? (bool)");

DEFINE_uint32(
    ws_max_subscriptions, 1024, "max subscriptions per connection (count)");

DEFINE_bool(cancel_on_disconnect, true, "cancel orders on disconnect? (bool)");

DEFINE_uint32(
    max_trades, uint32_t { 16384 }, "maximum trades for trade summary");

DEFINE_uint32(encode_buffer_size, 1048576, "encode buffer size");

DEFINE_uint32(decode_buffer_size, 10485760, "decode buffer size");
