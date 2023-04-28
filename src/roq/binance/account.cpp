/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/account.hpp"

namespace roq {
namespace binance {

// === VALIDATION ===

namespace {
static_assert(tools::Crypto::QUERY_BUFFER_LENGTH % 64 == 0);
}  // namespace

// === IMPLEMENTATION ===

Account::Account(Config const &config, std::string_view const &name)
    : name_{name}, crypto_{config.get_api_key(name_), config.get_secret(name_)},
      query_encode_buffer_(tools::Crypto::QUERY_BUFFER_LENGTH), cancel_order_request_buffer_(256) {
}

}  // namespace binance
}  // namespace roq
