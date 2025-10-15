/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "roq/binance/config.hpp"

#include "roq/binance/tools/crypto.hpp"

namespace roq {
namespace binance {

struct Account final {
  Account(Config const &, std::string_view const &name);

  Account(Account const &) = delete;

  std::string_view get_key() const { return crypto_.get_key(); }

  std::string_view create_session_logon_signature(std::chrono::milliseconds timestamp) {
    return crypto_.create_session_logon_signature(sign_buffer_, timestamp);
  }

  std::string const name;

 private:
  tools::Crypto crypto_;
  std::string sign_buffer_;
};

}  // namespace binance
}  // namespace roq
