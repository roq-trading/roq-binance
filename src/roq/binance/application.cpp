/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/application.hpp"

#include "roq/binance/config.hpp"
#include "roq/binance/gateway.hpp"
#include "roq/binance/settings.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const TYPE = server::Type::ORDER_MANAGEMENT;
}

// === IMPLEMENTATION ===

int Application::main(int, char **) {
  auto settings = Settings::create(TYPE);
  Config config{settings};
  auto context = server::create_io_context();
  server::Trading<Gateway>{settings, config, *context}.dispatch();
  return EXIT_SUCCESS;
}

}  // namespace binance
}  // namespace roq
