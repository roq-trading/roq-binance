/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/application.h"

namespace {
constexpr std::string_view DESCRIPTION = "Roq Binance Gateway";
}  // namespace

int main(int argc, char **argv) {
  return roq::binance::Application(
             argc,
             argv,
             DESCRIPTION,
             ROQ_BUILD_VERSION,
             ROQ_BUILD_TYPE,
             ROQ_GIT_DESCRIBE_HASH)
      .run();
}
