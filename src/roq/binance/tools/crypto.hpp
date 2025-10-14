/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "roq/utils/signature/context.hpp"
#include "roq/utils/signature/pkey.hpp"

namespace roq {
namespace binance {
namespace tools {

struct Crypto final {
  Crypto(std::string_view const &key, std::string_view const &secret);

  Crypto(Crypto &&) = delete;
  Crypto(Crypto const &) = delete;

  std::string_view get_key() const { return key_; }

  std::string_view create_query(std::span<std::byte> const &buffer, std::chrono::milliseconds now, std::string_view const &body);

  std::string_view create_headers() const { return headers_; }

  // WS API

  std::string_view create_ws_api_signature(std::string &result, std::string_view const &body);

 private:
  std::string const key_;
  std::string const headers_;
  //
  utils::signature::PKey pkey_;
  utils::signature::Context context_;
  std::vector<std::byte> digest_;
};

}  // namespace tools
}  // namespace binance
}  // namespace roq
