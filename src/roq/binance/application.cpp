/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/application.h"

#include "roq/binance/config.h"
#include "roq/binance/flags.h"
#include "roq/binance/gateway.h"

using namespace std::literals;  // NOLINT

namespace roq {
namespace binance {

int Application::main(int, char **) {
  LOG(INFO)(R"(Parse config_file="{}")"sv, Flags::config_file());
  Config config(Flags::config_file());
  VLOG(1)("config={}"sv, config);
  LOG(INFO)("Starting the gateway"sv);
  roq::server::Trading<Gateway>(ROQ_PACKAGE_NAME, config, server::RequestIdType::SEQUENTIAL, config)
      .dispatch();
  return EXIT_SUCCESS;
}

}  // namespace binance
}  // namespace roq
