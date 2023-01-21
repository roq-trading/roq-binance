/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "roq/core/message.hpp"

#include "roq/binance/config.hpp"

#include "roq/binance/tools/hasher.hpp"

namespace roq {
namespace binance {

struct Security final {
  Security(Config const &, std::string_view const &account);

  Security(Security &&) = delete;
  Security(Security const &) = delete;

  std::string_view get_account() const { return account_; }

  std::string_view create_query(std::chrono::milliseconds now, std::string_view const &body) {
    core::Message<char> buffer{query_encode_buffer_};
    return hasher_.create_query(buffer, now, body);
  }
  std::string_view create_query(std::chrono::milliseconds now) { return create_query(now, {}); }

  std::string_view create_headers() const { return hasher_.create_headers(); }

 private:
  std::string const account_;
  tools::Hasher hasher_;
  std::vector<char> query_encode_buffer_;
};

}  // namespace binance
}  // namespace roq
