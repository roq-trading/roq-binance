/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/security.hpp"

namespace roq {
namespace binance {

// === VALIDATION ===

namespace {
static_assert(tools::Hasher::QUERY_BUFFER_LENGTH % 64 == 0);
}  // namespace

// === IMPLEMENTATION ===

Security::Security(Config const &config, std::string_view const &account)
    : account_{account}, hasher_{config.get_api_key(account_), config.get_secret(account_)},
      query_encode_buffer_(tools::Hasher::QUERY_BUFFER_LENGTH) {
}

}  // namespace binance
}  // namespace roq
