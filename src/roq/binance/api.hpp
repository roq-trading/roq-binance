/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/binance/settings.hpp"

#include "roq/binance/json/side_effect_type.hpp"

namespace roq {
namespace binance {

struct API final {
  struct {
    std::string_view exchange_info;
    std::string_view depth;
  } market_data;
  struct {
    // std::string_view user_data_stream;
    // std::string_view account;
    // std::string_view open_orders;
    // std::string_view my_trades;
    // std::string_view order;
    // std::string_view order_cancel_replace;
    // margin - isolated
    std::string_view isolated_margin_user_data_stream;
    // margin - cross
    std::string_view cross_account;
    std::string_view margin_user_data_stream;
    // margin
    std::string_view margin_open_orders;
    std::string_view margin_order;
    json::SideEffectType margin_side_effect_type = {};
  } simple;
  struct {
    std::string_view ping_path;
    std::string_view listen_key;
    std::string_view balance;
    std::string_view margin_open_orders;
    std::string_view margin_my_trades;
    std::string_view margin_order;
    std::string_view margin_all_open_orders;
  } papi;

  // factory
  static API create(Settings const &);
};

}  // namespace binance
}  // namespace roq

template <>
struct fmt::formatter<roq::binance::API> {
  constexpr auto parse(format_parse_context &context) { return std::begin(context); }
  auto format(roq::binance::API const &value, format_context &context) const {
    using namespace std::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(market_data={{)"
        R"(exchange_info="{}", )"
        R"(depth="{}")"
        R"(}}, )"
        R"(simple={{)"
        // R"(user_data_stream="{}", )"
        // R"(account="{}", )"
        // R"(open_orders="{}", )"
        // R"(my_trades="{}", )"
        // R"(order="{}", )"
        // R"(order_cancel_replace="{}", )"
        R"(isolated_margin_user_data_stream="{}", )"
        R"(cross_account="{}", )"
        R"(margin_user_data_stream="{}", )"
        R"(margin_open_orders="{}", )"
        R"(margin_order="{}", )"
        R"(margin_side_effect_type="{}")"
        R"(}}, )"
        R"(papi={{)"
        R"(ping_path="{}", )"
        R"(listen_key="{}", )"
        R"(balance="{}", )"
        R"(margin_open_orders="{}", )"
        R"(margin_my_trades="{}", )"
        R"(margin_order="{}", )"
        R"(margin_all_open_orders="{}")"
        R"(}})"
        R"(}})"sv,
        // market_data
        value.market_data.exchange_info,
        value.market_data.depth,
        // simple
        // value.simple.user_data_stream,
        // value.simple.account,
        // value.simple.open_orders,
        // value.simple.my_trades,
        // value.simple.order,
        // value.simple.order_cancel_replace,
        value.simple.isolated_margin_user_data_stream,
        value.simple.cross_account,
        value.simple.margin_user_data_stream,
        value.simple.margin_open_orders,
        value.simple.margin_order,
        value.simple.margin_side_effect_type,
        // papi
        value.papi.ping_path,
        value.papi.listen_key,
        value.papi.balance,
        value.papi.margin_open_orders,
        value.papi.margin_my_trades,
        value.papi.margin_order,
        value.papi.margin_all_open_orders);
  }
};
