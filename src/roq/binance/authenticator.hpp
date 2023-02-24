/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "roq/binance/config.hpp"

#include "roq/binance/tools/crypto.hpp"

namespace roq {
namespace binance {

struct Authenticator final {
  Authenticator(Config const &, std::string_view const &account);

  Authenticator(Authenticator &&) = delete;
  Authenticator(Authenticator const &) = delete;

  inline std::string_view get_account() const { return account_; }

  inline std::string_view create_query(std::chrono::milliseconds now, std::string_view const &body) {
    return crypto_.create_query(query_encode_buffer_, now, body);
  }
  inline std::string_view create_query(std::chrono::milliseconds now) { return create_query(now, {}); }

  inline std::string_view create_headers() const { return crypto_.create_headers(); }

 private:
  std::string const account_;
  tools::Crypto crypto_;
  std::vector<std::byte> query_encode_buffer_;
};

}  // namespace binance
}  // namespace roq
