/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/application.hpp"

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
  log::info("Starting the gateway"sv);
  roq::server::Trading<Gateway>(ROQ_PACKAGE_NAME, ROQ_BUILD_NUMBER, {}, config).dispatch();
  return EXIT_SUCCESS;
}

}  // namespace binance
}  // namespace roq
