/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <gflags/gflags.h>

DECLARE_string(config_file);

DECLARE_string(exchange);

// rest
DECLARE_string(rest_uri);
DECLARE_uint32(rest_ping_freq_secs);
DECLARE_string(rest_ping_path);
DECLARE_uint32(rest_request_queue_depth);
DECLARE_uint32(rest_request_timeout_secs);
DECLARE_uint32(rest_rate_limit_interval_secs);
DECLARE_uint32(rest_rate_limit_max_requests);
DECLARE_uint32(rest_listen_key_refresh_secs);
DECLARE_uint32(rest_order_recv_window_msecs);
DECLARE_bool(rest_cancel_on_disconnect);

// ws
DECLARE_string(ws_uri);
DECLARE_uint32(ws_ping_freq_secs);
DECLARE_uint32(ws_max_subscriptions_per_stream);
DECLARE_uint32(ws_subscribe_depth_levels);
DECLARE_uint32(ws_subscribe_depth_freq_msecs);
DECLARE_bool(ws_subscribe_trade_details);

// XXX review
DECLARE_uint32(encode_buffer_size);
DECLARE_uint32(decode_buffer_size);

// external
DECLARE_string(name);
DECLARE_uint32(cache_mbp_max_depth);
