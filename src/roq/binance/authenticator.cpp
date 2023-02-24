/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/authenticator.hpp"

namespace roq {
namespace binance {

// === VALIDATION ===

namespace {
static_assert(tools::Crypto::QUERY_BUFFER_LENGTH % 64 == 0);
}  // namespace

// === IMPLEMENTATION ===

Authenticator::Authenticator(Config const &config, std::string_view const &account)
    : account_{account}, crypto_{config.get_api_key(account_), config.get_secret(account_)},
      query_encode_buffer_(tools::Crypto::QUERY_BUFFER_LENGTH) {
}

}  // namespace binance
}  // namespace roq
