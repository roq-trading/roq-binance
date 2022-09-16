/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/application.hpp"

#include "roq/io/engine/context_factory.hpp"

#include "roq/binance/config.hpp"
#include "roq/binance/flags.hpp"
#include "roq/binance/gateway.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

int Application::main(int, char **) {
  log::info(R"(Parse config_file="{}")"sv, Flags::config_file());
  Config config(Flags::config_file(), Flags::secrets_file());
  log::info<1>("config={}"sv, config);
  log::info("Prepare environment"sv);
  auto context = io::engine::ContextFactory::create(server::Flags::io_backend());
  log::info("Starting the gateway"sv);
  server::Settings settings{
      .package_name = ROQ_PACKAGE_NAME,
      .build_number = ROQ_BUILD_NUMBER,
      .api = {},
      .type = server::Type::ORDER_MANAGEMENT,
  };
  server::Trading<Gateway>(settings, config, *context).dispatch();
  return EXIT_SUCCESS;
}

}  // namespace binance
}  // namespace roq
