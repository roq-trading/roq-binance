/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/config.hpp"

#include <utility>

#include "roq/logging.hpp"

#include "roq/binance/json/cancel_restrictions.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const SUPPORTS = Mask{
    SupportType::TOP_OF_BOOK,
    SupportType::MARKET_BY_PRICE,
    SupportType::TRADE_SUMMARY,
    SupportType::STATISTICS,
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
    SupportType::CREATE_ORDER,
    SupportType::CANCEL_ORDER,
    SupportType::ORDER_ACK,
    SupportType::FUNDS,
};
auto const MBP_ALLOW_REMOVE_NON_EXISTING = true;
auto const OMS_REQUEST_ID_TYPE = RequestIdType::BASE64;
auto const OMS_CANCEL_ALL_ORDERS = Mask{
    Filter::ACCOUNT,
    Filter::EXCHANGE,
    Filter::SYMBOL,
};
}  // namespace

// === HELPERS ===

namespace {
auto create_gateway_settings(auto &settings) -> GatewaySettings {
  return {
      .supports = SUPPORTS,
      .mbp_max_depth = settings.mbp.max_depth,
      .mbp_tick_size_multiplier = NaN,
      .mbp_min_trade_vol_multiplier = NaN,
      .mbp_allow_remove_non_existing = MBP_ALLOW_REMOVE_NON_EXISTING,
      .mbp_allow_price_inversion = settings.mbp.allow_price_inversion,
      .mbp_checksum = settings.experimental.mbp_checksum,
      .oms_download_has_state = {},
      .oms_download_has_routing_id = {},
      .oms_request_id_type = OMS_REQUEST_ID_TYPE,
      .oms_cancel_all_orders = OMS_CANCEL_ALL_ORDERS,
  };
}
}  // namespace

// === IMPLEMENTATION ===

Config::Config(Settings const &settings)
    : exchange_{settings.exchange}, gateway_settings_{create_gateway_settings(settings)} {
  server::config::Reader::parse_file(*this, settings);
  log::info<1>("config={}"sv, *this);
}

Account const &Config::get_master_account() const {
  return master_account_;
}

std::string const &Config::get_api_key(Account const &account) const {
  auto iter = accounts.find(static_cast<std::string_view>(account));
  if (iter == std::end(accounts))
    log::fatal(R"(Unknown account="{}")"sv, account);
  return (*iter).second.login;
}

std::string const &Config::get_secret(Account const &account) const {
  auto iter = accounts.find(static_cast<std::string_view>(account));
  if (iter == std::end(accounts))
    log::fatal(R"(Unknown account="{}")"sv, account);
  return (*iter).second.secret;
}

void Config::dispatch(server::config::Handler &handler) const {
  handler(exchange_);
  handler(symbols);
  for (auto &iter : accounts)
    handler(iter.second);
  for (auto &user : users)
    handler(user);
  handler(gateway_settings_);
  for (auto &iter : rate_limits)
    handler(iter.second);
}

void Config::operator()(server::config::Symbols &&symbols) {
  (*this).symbols = std::move(symbols);
}

void Config::operator()(server::config::Account &&account) {
  if (account.master)
    master_account_ = account.name;
  accounts.emplace(account.name, std::move(account));
}

void Config::operator()(server::config::User &&user) {
  users.emplace_back(std::move(user));
}

void Config::operator()(server::config::RateLimit &&rate_limit) {
  rate_limits.emplace(rate_limit.name, std::move(rate_limit));
}

void Config::operator()(
    server::config::RequestTemplate request_template, std::string_view const &label, toml::table &table) {
  switch (request_template) {
    using enum server::config::RequestTemplate;
    case CREATE_ORDER: {
      json::CreateOrderTemplate create_order_template;
      for (auto &[k, v] : table) {
        auto key = static_cast<std::string_view>(k);
        if (key.compare("self_trade_prevention_mode"sv) == 0) {
          auto value = *v.template value<std::string_view>();
          create_order_template.self_trade_prevention_mode = value;
          if (create_order_template.self_trade_prevention_mode == json::SelfTradePreventionMode::UNKNOWN__)
            log::fatal(R"(Unknown: value="{}")"sv, value);
        } else {
          log::fatal(R"(Unexpected: key="{}")"sv, key);
        }
      }
      log::warn(R"(label="{}", create_order_template={})"sv, label, create_order_template);
      create_order_templates.try_emplace(label, std::move(create_order_template));
      break;
    }
    case MODIFY_ORDER:
      break;
    case CANCEL_ORDER: {
      json::CancelOrderTemplate cancel_order_template;
      for (auto &[k, v] : table) {
        auto key = static_cast<std::string_view>(k);
        if (key.compare("cancel_restrictions"sv) == 0) {
          auto value = *v.template value<std::string_view>();
          cancel_order_template.cancel_restrictions = value;
          if (cancel_order_template.cancel_restrictions == json::CancelRestrictions::UNKNOWN__)
            log::fatal(R"(Unknown: value="{}")"sv, value);
        } else if (key.compare("cancel_replace_mode"sv) == 0) {
          auto value = *v.template value<std::string_view>();
          cancel_order_template.cancel_replace_mode = value;
          if (cancel_order_template.cancel_replace_mode == json::CancelReplaceMode::UNKNOWN__)
            log::fatal(R"(Unknown: value="{}")"sv, value);
        } else {
          log::fatal(R"(Unexpected: key="{}")"sv, key);
        }
      }
      log::warn(R"(label="{}", cancel_order_template={})"sv, label, cancel_order_template);
      cancel_order_templates.try_emplace(label, std::move(cancel_order_template));
      break;
    }
  }
}

void Config::operator()(std::string_view const &key, toml::node &) {
  log::warn(R"(Unexpected: key="{}")"sv, key);
}

}  // namespace binance
}  // namespace roq
