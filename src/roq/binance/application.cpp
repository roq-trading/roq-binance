/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/application.hpp"

#include "roq/binance/config.hpp"
#include "roq/binance/flags.hpp"
#include "roq/binance/gateway.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const SETTINGS = server::Settings{
    .package_name = ROQ_PACKAGE_NAME,
    .build_number = ROQ_BUILD_NUMBER,
    .api = {},
    .type = server::Type::ORDER_MANAGEMENT,
};
}

// === IMPLEMENTATION ===

int Application::main(int, char **) {
  Flags2 flags;
  Config config{flags};
  auto context = server::create_io_context();
  server::Trading<Gateway>{SETTINGS, config, *context}.dispatch();
  return EXIT_SUCCESS;
}

}  // namespace binance
}  // namespace roq
