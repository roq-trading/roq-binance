/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "roq/binance/config.hpp"

#include "roq/binance/tools/hasher.hpp"

namespace roq {
namespace binance {

class Security final {
 public:
  Security(Config const &, std::string_view const &account);

  Security(Security &&) = delete;
  Security(Security const &) = delete;

  std::string_view get_account() const { return account_; }

  std::string_view create_query(std::string_view const &body) { return hasher_.create_query(body); }
  std::string_view create_query() { return create_query({}); }

  std::string_view create_headers() const { return hasher_.create_headers(); }

 private:
  std::string const account_;
  tools::Hasher hasher_;
};

}  // namespace binance
}  // namespace roq
