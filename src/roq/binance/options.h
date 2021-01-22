/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <absl/flags/declare.h>

#include <cstdint>
#include <string>

ABSL_DECLARE_FLAG(std::string, config_file);

ABSL_DECLARE_FLAG(std::string, exchange);

// rest
ABSL_DECLARE_FLAG(std::string, rest_uri);
ABSL_DECLARE_FLAG(uint32_t, rest_ping_freq_secs);
ABSL_DECLARE_FLAG(std::string, rest_ping_path);
ABSL_DECLARE_FLAG(uint32_t, rest_request_queue_depth);
ABSL_DECLARE_FLAG(uint32_t, rest_request_timeout_secs);
ABSL_DECLARE_FLAG(uint32_t, rest_rate_limit_interval_secs);
ABSL_DECLARE_FLAG(uint32_t, rest_rate_limit_max_requests);
ABSL_DECLARE_FLAG(uint32_t, rest_listen_key_refresh_secs);
ABSL_DECLARE_FLAG(uint32_t, rest_order_recv_window_msecs);
ABSL_DECLARE_FLAG(bool, rest_cancel_on_disconnect);

// ws
ABSL_DECLARE_FLAG(std::string, ws_uri);
ABSL_DECLARE_FLAG(uint32_t, ws_ping_freq_secs);
ABSL_DECLARE_FLAG(uint32_t, ws_max_subscriptions_per_stream);
ABSL_DECLARE_FLAG(uint32_t, ws_subscribe_depth_levels);
ABSL_DECLARE_FLAG(uint32_t, ws_subscribe_depth_freq_msecs);
ABSL_DECLARE_FLAG(bool, ws_subscribe_trade_details);

// XXX review
ABSL_DECLARE_FLAG(uint32_t, encode_buffer_size);
ABSL_DECLARE_FLAG(uint32_t, decode_buffer_size);

// external
ABSL_DECLARE_FLAG(std::string, name);
ABSL_DECLARE_FLAG(uint32_t, cache_mbp_max_depth);
