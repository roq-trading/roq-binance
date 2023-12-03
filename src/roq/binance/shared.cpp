/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/shared.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === IMPLEMENTATION ===

Shared::Shared(server::Dispatcher &dispatcher, Settings const &settings, Config const &config)
    : dispatcher_{dispatcher}, settings{settings},
      rate_limiter{settings.common.request_limit, settings.common.request_limit_interval},
      symbols{settings.ws.max_subscriptions_per_stream}, depth_request_queue{settings.ws.mbp_request_delay},
      create_order_templates{config.create_order_templates}, cancel_order_templates{config.cancel_order_templates} {
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

}  // namespace binance
}  // namespace roq
