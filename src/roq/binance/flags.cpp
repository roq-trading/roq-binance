/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/flags.h"

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

#include <string>

#include "roq/core/flags/non_empty.h"
#include "roq/core/flags/non_zero.h"
#include "roq/core/flags/path.h"
#include "roq/core/flags/uri.h"

using namespace std::literals;  // NOLINT

ABSL_FLAG(  //
    roq::core::flags::Path<std::string>,
    config_file,
    {},
    "config file (path)"sv);

ABSL_FLAG(  //
    roq::core::flags::NonEmpty<std::string>,
    exchange,
    "binance"s,
    "exchange identifier (string)"sv);

// rest

ABSL_FLAG(  //
    roq::core::flags::URI<std::string>,
    rest_uri,
    "https://testnet.binance.com/api/v1"s,
    "REST end-point (URI)"sv);

ABSL_FLAG(  //
    uint32_t,
    rest_ping_freq_secs,
    uint32_t{5},
    "ping frequency (seconds)"sv);

ABSL_FLAG(  //
    roq::core::flags::Path<std::string>,
    rest_ping_path,
    "/api/v3/time"s,
    "URI path used for REST connection keep-alive messages"sv);

ABSL_FLAG(  //
    uint32_t,
    rest_request_queue_depth,
    uint32_t{5},
    "request: max queue depth"sv);

ABSL_FLAG(  //
    uint32_t,
    rest_request_timeout_secs,
    uint32_t{30},
    "request: timeout (seconds)"sv);

ABSL_FLAG(  //
    uint32_t,
    rest_rate_limit_interval_secs,
    uint32_t{60},
    "rate limit: monitor interval (seconds)"sv);

ABSL_FLAG(  //
    uint32_t,
    rest_rate_limit_max_requests,
    uint32_t{1200},
    "rate limit: max requests (per interval)"sv);

ABSL_FLAG(  //
    uint32_t,
    rest_listen_key_refresh_secs,
    uint32_t{1800},
    "listen key refresh period (seconds)"sv);

ABSL_FLAG(  //
    uint32_t,
    rest_order_recv_window_msecs,
    uint32_t{5000},
    "receive window (milliseconds), please refer to Binance documentation!"sv);

ABSL_FLAG(  //
    bool,
    rest_cancel_on_disconnect,
    false,
    "cancel orders on disconnect? (bool)"sv);

// ws

ABSL_FLAG(  //
    roq::core::flags::URI<std::string>,
    ws_uri,
    "wss://testnet.binance.com/realtime"s,
    "WebSocket end-point (URI)"sv);

ABSL_FLAG(  //
    uint32_t,
    ws_ping_freq_secs,
    uint32_t{5},
    "ping frequency (seconds)"sv);

ABSL_FLAG(  //
    roq::core::flags::NonZero<uint32_t>,
    ws_max_subscriptions_per_stream,
    uint32_t{512},
    "max subscriptions per connection (count)"sv);

ABSL_FLAG(  //
    uint32_t,
    ws_subscribe_depth_levels,
    uint32_t{20},
    "depth levels (count)"sv);

ABSL_FLAG(  //
    uint32_t,
    ws_subscribe_depth_freq_msecs,
    uint32_t{100},
    "depth update frequency (milliseconds)"sv);

ABSL_FLAG(  //
    bool,
    ws_subscribe_trade_details,
    false,
    "report individual matches for trade summary? (bool)"sv);

// XXX review

ABSL_FLAG(  //
    roq::core::flags::NonZero<uint32_t>,
    encode_buffer_size,
    uint32_t{1048576},
    "encode buffer size"sv);

ABSL_FLAG(  //
    roq::core::flags::NonZero<uint32_t>,
    decode_buffer_size,
    uint32_t{10485760},
    "decode buffer size"sv);

// external

ABSL_DECLARE_FLAG(roq::core::flags::NonEmpty<std::string>, name);
ABSL_DECLARE_FLAG(roq::core::flags::NonZero<uint32_t>, cache_mbp_max_depth);

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
  static const uint32_t result = absl::GetFlag(FLAGS_rest_rate_limit_interval_secs);
  return result;
}

uint32_t Flags::rest_rate_limit_max_requests() {
  static const uint32_t result = absl::GetFlag(FLAGS_rest_rate_limit_max_requests);
  return result;
}

uint32_t Flags::rest_listen_key_refresh_secs() {
  static const uint32_t result = absl::GetFlag(FLAGS_rest_listen_key_refresh_secs);
  return result;
}

uint32_t Flags::rest_order_recv_window_msecs() {
  static const uint32_t result = absl::GetFlag(FLAGS_rest_order_recv_window_msecs);
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
  static const uint32_t result = absl::GetFlag(FLAGS_ws_max_subscriptions_per_stream);
  return result;
}

uint32_t Flags::ws_subscribe_depth_levels() {
  static const uint32_t result = absl::GetFlag(FLAGS_ws_subscribe_depth_levels);
  return result;
}

uint32_t Flags::ws_subscribe_depth_freq_msecs() {
  static const uint32_t result = absl::GetFlag(FLAGS_ws_subscribe_depth_freq_msecs);
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
