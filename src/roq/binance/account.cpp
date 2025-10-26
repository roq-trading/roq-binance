/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/account.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === HELPERS ===

namespace {
template <typename R>
auto create_crypto(auto &config, auto &name, auto margin_mode) {
  using result_type = std::remove_cvref_t<R>;
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
  return result_type{key, secret, margin_mode};
}
}  // namespace

// === IMPLEMENTATION ===

Account::Account(Config const &config, std::string_view const &name, MarginMode margin_mode)
    : name{name}, margin_mode{margin_mode}, crypto_{create_crypto<decltype(crypto_)>(config, name, margin_mode)},
      query_encode_buffer_(tools::Crypto::QUERY_BUFFER_LENGTH) {
}

}  // namespace binance
}  // namespace roq
