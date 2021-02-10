/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/application.h"

using namespace std::literals;  // NOLINT

namespace {
static const auto DESCRIPTION = "Roq Binance Gateway"sv;
}  // namespace

int main(int argc, char **argv) {
  return roq::binance::Application(
             argc, argv, DESCRIPTION, ROQ_BUILD_VERSION, ROQ_BUILD_TYPE, ROQ_GIT_DESCRIBE_HASH)
      .run();
}
