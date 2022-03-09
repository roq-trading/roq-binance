/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/security.hpp"

namespace roq {
namespace binance {

Security::Security(const Config &config, const std::string_view &account)
    : account_(account), hasher_(config.get_api_key(account_), config.get_secret(account_)) {
}

}  // namespace binance
}  // namespace roq
