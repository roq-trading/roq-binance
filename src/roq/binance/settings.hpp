/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include "roq/compat/fmt.hpp"

#include <fmt/format.h>

#include "roq/server/flags/settings.hpp"

#include "roq/binance/flags/download.hpp"
#include "roq/binance/flags/flags.hpp"
#include "roq/binance/flags/mbp.hpp"
#include "roq/binance/flags/misc.hpp"
#include "roq/binance/flags/oms.hpp"
#include "roq/binance/flags/request.hpp"
#include "roq/binance/flags/rest.hpp"
#include "roq/binance/flags/ws.hpp"
#include "roq/binance/flags/ws_api.hpp"

namespace roq {
namespace binance {

struct Settings final : public server::flags::Settings, public flags::Flags {
  explicit Settings(args::Parser const &);

  flags::Misc misc;
  flags::REST rest;
  flags::WS ws;
  flags::WS_API ws_api_2;  // note! overlapping with flags::Flags
  flags::Download download;
  flags::MBP mbp;
  flags::OMS oms;
  flags::Request request;
};

}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::Settings> {
  constexpr auto parse(format_parse_context &context) { return std::begin(context); }
  auto format(roq::binance::Settings const &value, format_context &context) const {
    using namespace std::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(exchange="{}", )"
        R"(use_ws_api={}, )"
        R"(misc={}, )"
        R"(rest={}, )"
        R"(ws={}, )"
        R"(ws_api={}, )"
        R"(download={}, )"
        R"(mbp={}, )"
        R"(oms={}, )"
        R"(request={}, )"
        R"(server={})"
        R"(}})"sv,
        value.exchange,
        value.ws_api,
        value.misc,
        value.rest,
        value.ws,
        value.ws_api_2,
        value.download,
        value.mbp,
        value.oms,
        value.request,
        static_cast<roq::server::Settings const &>(value));
  }
};
