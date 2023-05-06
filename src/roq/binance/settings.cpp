/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/settings.hpp"

#include "roq/binance/flags/flags.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

Settings Settings::create(server::Type type) {
  auto settings = server::create_settings(type, ROQ_PACKAGE_NAME, ROQ_BUILD_NUMBER);
  return {settings};
}

}  // namespace binance
}  // namespace roq
