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
      VERSION).run();
}
