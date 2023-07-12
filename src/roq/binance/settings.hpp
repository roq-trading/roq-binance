/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/chrono.h>
#include <fmt/compile.h>
#include <fmt/format.h>

#include "roq/server/flags/settings.hpp"

#include "roq/binance/flags/common.hpp"
#include "roq/binance/flags/flags.hpp"
#include "roq/binance/flags/rest.hpp"
#include "roq/binance/flags/ws.hpp"
#include "roq/binance/flags/ws_api.hpp"

namespace roq {
namespace binance {

struct Settings final : public server::flags::Settings {
  explicit Settings(args::Parser const &, server::Type);

  std::string_view exchange;
  bool use_ws_api = {};

  flags::Common__flags common;
  flags::REST__flags rest;
  flags::WS__flags ws;
  flags::WS_API__flags ws_api;
};

}  // namespace binance
}  // namespace roq

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
        R"(use_ws_api={}, )"
        R"(common={}, )"
        R"(rest={}, )"
        R"(ws={}, )"
        R"(ws_api={}, )"
        R"(server={})"
        R"(}})"_cf,
        value.exchange,
        value.use_ws_api,
        value.common,
        value.rest,
        value.ws,
        value.ws_api,
        static_cast<roq::server::Settings const &>(value));
  }
};
