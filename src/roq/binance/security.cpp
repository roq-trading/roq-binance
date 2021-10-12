/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/security.h"

namespace roq {
namespace binance {

Security::Security(const Config &config, const std::string_view &account)
    : account_(account), key_(config.get_api_key(account_)), hasher_(config.get_secret(account_)) {
}

std::pair<std::string, std::string> Security::create_signature(std::chrono::nanoseconds now) {
  return hasher_.create_signature(now);
}

}  // namespace binance
}  // namespace roq
