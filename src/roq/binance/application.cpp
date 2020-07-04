/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/application.h"

#include "roq/binance/config.h"
#include "roq/binance/gateway.h"
#include "roq/binance/options.h"

namespace roq {
namespace binance {

int Application::main(int, char **) {
  LOG(INFO)("Parse configuration");
  Config config(FLAGS_config_file);
  VLOG(1)(
      "config={}",
      config);
  LOG(INFO)("Starting the gateway");
  roq::server::Trading<Gateway>(
      PACKAGE_NAME,
      config,
      server::RequestIdType::SEQUENTIAL,
      config).dispatch();
  return EXIT_SUCCESS;
}

}  // namespace binance
}  // namespace roq
