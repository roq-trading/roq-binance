/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/settings.hpp"

#include "roq/logging.hpp"

#include "roq/server/flags/settings.hpp"

#include "roq/binance/flags/flags.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

Settings::Settings(server::Type type)
    : server::Settings{server::flags::create_settings(type, ROQ_PACKAGE_NAME, ROQ_BUILD_NUMBER)},
      exchange{flags::Flags::exchange()}, use_ws_api{flags::Flags::ws_api()} {
  log::debug("settings={}"sv, *this);
}

}  // namespace binance
}  // namespace roq
