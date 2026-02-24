/* Copyright (c) 2017-2026, Hans Erik Thrane */

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
  Account(Config const &, std::string_view const &name, MarginMode);

  Account(Account const &) = delete;

  std::string_view get_key() const { return crypto_.get_key(); }

  // ed25519

  std::string_view get_rest_headers_new() const { return crypto_.get_rest_headers_new(); }

  std::string_view create_session_logon_signature(std::chrono::milliseconds timestamp) {
    return crypto_.create_session_logon_signature(sign_buffer_, timestamp);
  }

  std::string_view create_rest_signature_body_new(std::chrono::milliseconds now_utc, std::string_view const &body) {
    return crypto_.create_rest_signature_body_new(query_encode_buffer_, now_utc, body);
  }

  // classic

  std::string_view get_rest_headers_old() const { return crypto_.get_rest_headers_old(); }

  std::string_view create_rest_signature(std::chrono::milliseconds now_utc) { return crypto_.create_rest_signature(query_encode_buffer_, now_utc); }

  std::string_view create_rest_signature_body(std::chrono::milliseconds now_utc, std::string_view const &body) {
    return crypto_.create_rest_signature_body(query_encode_buffer_, now_utc, body);
  }

  std::string create_rest_signature_query(std::chrono::milliseconds now_utc, std::string_view const &query) {
    return crypto_.create_rest_signature_query(now_utc, query);
  }

  std::string const name;
  MarginMode const margin_mode;

 private:
  tools::Crypto crypto_;
  std::string sign_buffer_;
  std::vector<std::byte> query_encode_buffer_;
};

}  // namespace binance
}  // namespace roq
