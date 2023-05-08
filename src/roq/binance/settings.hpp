/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/compile.h>
#include <fmt/format.h>

#include "roq/server.hpp"

namespace roq {
namespace binance {

namespace detail {
struct Common final {
  uint32_t request_limit = {};
  std::chrono::nanoseconds request_limit_interval = {};
  uint32_t encode_buffer_size = {};
  uint32_t decode_buffer_size = {};
  bool mbp_allow_price_inversion = {};
  uint16_t mbp_max_depth = {};
  bool cancel_replace_stop_on_failure = {};
};
struct REST final {
  std::span<std::string const> network_interfaces;
  roq::io::web::URI const &uri;
  roq::io::web::URI const &proxy;
  std::chrono::nanoseconds ping_freq = {};
  std::string_view ping_path;
  std::chrono::nanoseconds request_timeout = {};
  bool terminate_on_403 = {};
  std::chrono::nanoseconds back_off_delay = {};
  std::chrono::nanoseconds listen_key_refresh = {};
  std::chrono::nanoseconds order_recv_window = {};
  bool cancel_on_disconnect = {};
};
struct WS final {
  roq::io::web::URI const &uri;
  std::chrono::nanoseconds ping_freq = {};
  uint32_t max_subscriptions_per_stream = {};
  uint32_t subscribe_depth_levels = {};
  std::chrono::nanoseconds subscribe_depth_freq = {};
  std::chrono::nanoseconds mbp_request_delay = {};
  uint32_t mbp_request_max_retries = {};
  bool subscribe_trade_details = {};
  bool enable_secondary = {};
};
struct WSAPI final {
  bool enable = {};
  std::span<std::string const> network_interfaces;
  roq::io::web::URI const &uri;
  std::chrono::nanoseconds ping_freq = {};
};
}  // namespace detail

struct Settings final : public server::Settings {
  explicit Settings(server::Type);

  std::string_view exchange;
  detail::Common common;
  detail::REST rest;
  detail::WS ws;
  detail::WSAPI ws_api;
};

}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::detail::Common> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::binance::detail::Common const &value, Context &context) const {
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(request_limit={}, )"
        R"(request_limit_interval={}, )"
        R"(encode_buffer_size={}, )"
        R"(decode_buffer_size={}, )"
        R"(mbp_allow_price_inversion={}, )"
        R"(mbp_max_depth={}, )"
        R"(cancel_replace_stop_on_failure={})"
        R"(}})"_cf,
        value.request_limit,
        value.request_limit_interval,
        value.encode_buffer_size,
        value.decode_buffer_size,
        value.mbp_allow_price_inversion,
        value.mbp_max_depth,
        value.cancel_replace_stop_on_failure);
  }
};

template <>
struct fmt::formatter<roq::binance::detail::REST> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::binance::detail::REST const &value, Context &context) const {
    using namespace std::literals;
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(network_interfaces=[{}], )"
        R"(uri={}, )"
        R"(proxy={}, )"
        R"(ping_freq={}, )"
        R"(ping_path="{}", )"
        R"(request_timeout={}, )"
        R"(terminate_on_403={}, )"
        R"(back_off_delay={}, )"
        R"(listen_key_refresh={}, )"
        R"(order_recv_window={}, )"
        R"(cancel_on_disconnect={})"
        R"(}})"_cf,
        fmt::join(value.network_interfaces, ", "sv),
        value.uri,
        value.proxy,
        value.ping_freq,
        value.ping_path,
        value.request_timeout,
        value.terminate_on_403,
        value.back_off_delay,
        value.listen_key_refresh,
        value.order_recv_window,
        value.cancel_on_disconnect);
  }
};

template <>
struct fmt::formatter<roq::binance::detail::WS> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::binance::detail::WS const &value, Context &context) const {
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(uri={}, )"
        R"(ping_freq={}, )"
        R"(max_subscriptions_per_stream={}, )"
        R"(subscribe_depth_levels={}, )"
        R"(subscribe_depth_freq={}, )"
        R"(mbp_request_delay={}, )"
        R"(mbp_request_max_retries={}, )"
        R"(subscribe_trade_details={}, )"
        R"(enable_secondary={})"
        R"(}})"_cf,
        value.uri,
        value.ping_freq,
        value.max_subscriptions_per_stream,
        value.subscribe_depth_levels,
        value.subscribe_depth_freq,
        value.mbp_request_delay,
        value.mbp_request_max_retries,
        value.subscribe_trade_details,
        value.enable_secondary);
  }
};

template <>
struct fmt::formatter<roq::binance::detail::WSAPI> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::binance::detail::WSAPI const &value, Context &context) const {
    using namespace std::literals;
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(enable={}, )"
        R"(network_interfaces=[{}], )"
        R"(uri={}, )"
        R"(ping_freq={})"
        R"(}})"_cf,
        value.enable,
        fmt::join(value.network_interfaces, ", "sv),
        value.uri,
        value.ping_freq);
  }
};

template <>
struct fmt::formatter<roq::binance::Settings> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::binance::Settings const &value, Context &context) const {
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(exchange="{}", )"
        R"(common={}, )"
        R"(rest={}, )"
        R"(ws={}, )"
        R"(ws_api={}, )"
        R"(server={})"
        R"(}})"_cf,
        value.exchange,
        value.common,
        value.rest,
        value.ws,
        value.ws_api,
        static_cast<roq::server::Settings const &>(value));
  }
};
