/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/flags/settings.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace flags {

Settings::Settings(args::Parser const &args)
    : server::flags::Settings{args, ROQ_PACKAGE_NAME, ROQ_BUILD_NUMBER}, flags::Flags{flags::Flags::create()}, misc{flags::Misc::create()},
      rest{flags::REST::create()}, ws{flags::WS::create()}, ws_api_2{flags::WS_API::create()}, mbp{flags::MBP::create()}, oms{flags::OMS::create()},
      request{flags::Request::create()} {
  log::info("settings={}"sv, *this);
}

}  // namespace flags
}  // namespace binance
}  // namespace roq
