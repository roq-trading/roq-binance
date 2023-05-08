/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/settings.hpp"

#include "roq/logging.hpp"

#include "roq/binance/flags/flags.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

Settings::Settings(server::Type type)
    : server::Settings{server::create_settings(type, ROQ_PACKAGE_NAME, ROQ_BUILD_NUMBER)},
      exchange{flags::Flags::exchange()},
      common{
          .request_limit = flags::Flags::request_limit(),
          .request_limit_interval = flags::Flags::request_limit_interval(),
          .encode_buffer_size = flags::Flags::encode_buffer_size(),
          .decode_buffer_size = flags::Flags::decode_buffer_size(),
          .mbp_allow_price_inversion = flags::Flags::mbp_allow_price_inversion(),
          .mbp_max_depth = flags::Flags::mbp_max_depth(),
          .cancel_replace_stop_on_failure = flags::Flags::cancel_replace_stop_on_failure(),
      },
      rest{
          .network_interfaces = flags::Flags::rest_network_interfaces(),
          .uri = flags::Flags::rest_uri(),
          .proxy = flags::Flags::rest_proxy(),
          .ping_freq = flags::Flags::rest_ping_freq(),
          .ping_path = flags::Flags::rest_ping_path(),
          .request_timeout = flags::Flags::rest_request_timeout(),
          .terminate_on_403 = flags::Flags::rest_terminate_on_403(),
          .back_off_delay = flags::Flags::rest_back_off_delay(),
          .listen_key_refresh = flags::Flags::rest_listen_key_refresh(),
          .order_recv_window = flags::Flags::rest_order_recv_window(),
          .cancel_on_disconnect = flags::Flags::rest_cancel_on_disconnect(),
      },
      ws{
          .uri = flags::Flags::ws_uri(),
          .ping_freq = flags::Flags::ws_ping_freq(),
          .max_subscriptions_per_stream = flags::Flags::ws_max_subscriptions_per_stream(),
          .subscribe_depth_levels = flags::Flags::ws_subscribe_depth_levels(),
          .subscribe_depth_freq = flags::Flags::ws_subscribe_depth_freq(),
          .mbp_request_delay = flags::Flags::ws_mbp_request_delay(),
          .mbp_request_max_retries = flags::Flags::ws_mbp_request_max_retries(),
          .subscribe_trade_details = flags::Flags::ws_subscribe_trade_details(),
          .enable_secondary = flags::Flags::ws_enable_secondary(),
      },
      ws_api{
          .enable = flags::Flags::ws_api(),
          .network_interfaces = flags::Flags::ws_api_network_interfaces(),
          .uri = flags::Flags::ws_api_uri(),
          .ping_freq = flags::Flags::ws_api_ping_freq(),
      } {
  log::debug("settings={}"sv, *this);
}

}  // namespace binance
}  // namespace roq
