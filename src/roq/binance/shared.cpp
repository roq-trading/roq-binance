/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/shared.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === HELPERS ===

namespace {
auto create_sequencer(auto &settings) {
  auto options = market::mbp::Sequencer::Options{
      .timeout = settings.common.mbp_sequencer_timeout,
      .max_updates = {},
  };
  return market::mbp::Sequencer{options};
}
}  // namespace

// === IMPLEMENTATION ===

Shared::Shared(server::Dispatcher &dispatcher, Settings const &settings, Config const &config)
    : dispatcher_{dispatcher}, settings{settings},
      rate_limiter{settings.common.request_limit, settings.common.request_limit_interval},
      symbols{settings.ws.max_subscriptions_per_stream}, depth_request_queue{settings.ws.mbp_request_delay},
      create_order_templates{config.create_order_templates}, cancel_order_templates{config.cancel_order_templates} {
}

Shared::Instrument &Shared::get_instrument(std::string_view const &symbol) {
  auto iter = instruments_.find(symbol);
  if (iter == std::end(instruments_)) [[unlikely]] {
    auto res = instruments_.try_emplace(symbol, settings);
    assert(res.second);
    iter = res.first;
  }
  return (*iter).second;
}

json::CreateOrderTemplate const &Shared::get_create_order_template(std::string_view const &name) {
  if (std::empty(name)) {
    static auto const empty = json::CreateOrderTemplate{};
    return empty;
  }
  auto iter = create_order_templates.find(name);
  if (iter != std::end(create_order_templates))
    return (*iter).second;
  throw roq::oms::Rejected{Origin::GATEWAY, Error::INVALID_REQUEST_TEMPLATE, "create_order_template"sv};
}

json::CancelOrderTemplate const &Shared::get_cancel_order_template(std::string_view const &name) {
  if (std::empty(name)) {
    static auto const empty = json::CancelOrderTemplate{};
    return empty;
  }
  auto iter = cancel_order_templates.find(name);
  if (iter != std::end(cancel_order_templates))
    return (*iter).second;
  throw roq::oms::Rejected{Origin::GATEWAY, Error::INVALID_REQUEST_TEMPLATE, "cancel_order_template"sv};
}

// instrument

Shared::Instrument::Instrument(Settings const &settings) : sequencer{create_sequencer(settings)} {
}

}  // namespace binance
}  // namespace roq
