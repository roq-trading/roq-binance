/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <fmt/ranges.h>

#include <toml++/toml.h>

#include <string>
#include <string_view>
#include <vector>

#include "roq/logging.hpp"
#include "roq/server.hpp"

#include "roq/binance/flags.hpp"

#include "roq/binance/json/cancel_order_template.hpp"
#include "roq/binance/json/create_order_template.hpp"

namespace roq {
namespace binance {

struct Config final : public server::Config, public server::ConfigReader::Handler {
  struct Options final {
    std::string_view exchange;
    uint16_t mbp_max_depth = {};
    bool mbp_allow_price_inversion = {};
    bool mbp_checksum = {};
  };

  explicit Config(Options const &);
  explicit Config(Flags2 const &);

  Account const &get_master_account() const;

  std::string const &get_api_key(Account const &) const;
  std::string const &get_secret(Account const &) const;

 protected:
  // server::Config
  void dispatch(server::Config::Handler &) const override;

  // server::ConfigReader::Handler
  void operator()(server::Symbols &&) override;
  void operator()(server::Account &&) override;
  void operator()(server::User &&) override;
  void operator()(server::RateLimit &&) override;
  void operator()(server::RequestTemplate, std::string_view const &label, toml::table &) override;
  void operator()(std::string_view const &key, toml::node &) override;

 private:
  std::string const exchange_;
  uint16_t const mbp_max_depth_;
  bool const mbp_allow_price_inversion_;
  bool const mbp_checksum_;

 public:
  server::Users users;
  server::Symbols symbols;
  server::Accounts accounts;
  Account master_account_;
  server::RateLimits rate_limits;
  absl::flat_hash_map<std::string, json::CreateOrderTemplate> create_order_templates;
  absl::flat_hash_map<std::string, json::CancelOrderTemplate> cancel_order_templates;
};

/*
 * REST API
 * https://api-public.sandbox.pro.binance.com
 *
 * Websocket Feed
 * wss://ws-feed-public.sandbox.pro.binance.com
 *
 * FIX API
 * tcp+ssl://fix-public.sandbox.pro.binance.com:4198
 */

}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::Config> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::binance::Config const &value, Context &context) const {
    using namespace std::literals;
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(symbols={}, )"
        R"(accounts=[{}], )"
        R"(master_account="{}", )"
        R"(users=[{}], )"
        R"(rate_limits=[{}], )"
        R"(create_order_templates=[{}], )"
        R"(cancel_order_templates=[{}])"
        R"(}})"_cf,
        value.symbols,
        fmt::join(value.accounts, ", "sv),
        value.master_account_,
        fmt::join(value.users, ", "sv),
        fmt::join(value.rate_limits, ", "sv),
        fmt::join(value.create_order_templates, ", "sv),
        fmt::join(value.cancel_order_templates, ", "sv));
  }
};
