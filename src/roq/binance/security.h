/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "roq/binance/config.h"

#include "roq/binance/tools/hasher.h"

namespace roq {
namespace binance {

class Security final {
 public:
  Security(const Config &, const std::string_view &account);

  Security(Security &&) = delete;
  Security(const Security &) = delete;

  std::string_view get_account() const { return account_; }

  std::string create_query();
  std::string create_headers() const;

 private:
  const std::string account_;
  tools::Hasher hasher_;
};

}  // namespace binance
}  // namespace roq
