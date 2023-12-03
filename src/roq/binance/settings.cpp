/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/settings.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

Settings::Settings(args::Parser const &args)
    : server::flags::Settings{args, ROQ_PACKAGE_NAME, ROQ_BUILD_NUMBER}, flags::Flags{flags::Flags::create()},
      common{flags::Common::create()}, rest{flags::REST::create()}, ws{flags::WS::create()},
      ws_api_2{flags::WS_API::create()} {
  log::debug("settings={}"sv, *this);
}

}  // namespace binance
}  // namespace roq
