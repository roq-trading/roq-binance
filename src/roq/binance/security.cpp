/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/security.h"

namespace roq {
namespace binance {

Security::Security(const Config &config, const std::string_view &account)
    : account_(account), hasher_(config.get_api_key(account_), config.get_secret(account_)) {
}

std::string Security::create_query() {
  return hasher_.create_query();
}

std::string Security::create_headers() const {
  return hasher_.create_headers();
}

}  // namespace binance
}  // namespace roq
