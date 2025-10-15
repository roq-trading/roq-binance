/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/account.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === HELPERS ===

namespace {
auto create_crypto(auto &config, auto &name) -> tools::Crypto {
  auto ready = true;
  auto key = config.get_api_key(name);
  if (std::empty(key)) {
    ready = false;
    log::warn(R"(Unexpected: missing key for name="{}")"sv, name);
  }
  auto secret = config.get_secret(name);
  if (std::empty(secret)) {
    ready = false;
    log::warn(R"(Unexpected: missing secret for name="{}")"sv, name);
  }
  if (!ready) {
    log::fatal("Invalid config"sv);
  }
  return {key, secret};
}
}  // namespace

// === IMPLEMENTATION ===

Account::Account(Config const &config, std::string_view const &name) : name{name}, crypto_{create_crypto(config, name)} {
}

}  // namespace binance
}  // namespace roq
