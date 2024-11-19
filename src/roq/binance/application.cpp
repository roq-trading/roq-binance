/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/application.hpp"

#include "roq/binance/config.hpp"
#include "roq/binance/gateway.hpp"
#include "roq/binance/settings.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === IMPLEMENTATION ===

int Application::main(args::Parser const &args) {
  Settings settings{args};
  Config config{settings};
  auto context = server::create_io_context(settings);
  server::Trading<Gateway>{settings, config, *context}.dispatch();
  return EXIT_SUCCESS;
}

}  // namespace binance
}  // namespace roq
