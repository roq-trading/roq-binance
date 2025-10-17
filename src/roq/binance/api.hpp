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
    std::string_view listen_key;
    // margin - isolated
    std::string_view isolated_margin_user_data_stream;
    // margin - cross
    std::string_view cross_account;
    std::string_view margin_user_data_stream;
    // margin
    std::string_view margin_open_orders;
    std::string_view margin_my_trades;
    std::string_view margin_order;
  } sapi;
  struct {
    std::string_view ping_path;
    std::string_view listen_key;
    std::string_view balance;
    std::string_view margin_open_orders;
    std::string_view margin_my_trades;
    std::string_view margin_order;
    std::string_view margin_all_open_orders;
  } papi;
  json::SideEffectType margin_side_effect_type = {};

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
        R"(sapi={{)"
        R"(listen_key="{}", )"
        R"(isolated_margin_user_data_stream="{}", )"
        R"(cross_account="{}", )"
        R"(margin_user_data_stream="{}", )"
        R"(margin_open_orders="{}", )"
        R"(margin_order="{}")"
        R"(}}, )"
        R"(papi={{)"
        R"(ping_path="{}", )"
        R"(listen_key="{}", )"
        R"(balance="{}", )"
        R"(margin_open_orders="{}", )"
        R"(margin_my_trades="{}", )"
        R"(margin_order="{}", )"
        R"(margin_all_open_orders="{}")"
        R"(}}, )"
        R"(margin_side_effect_type="{}")"
        R"(}})"sv,
        // market_data
        value.market_data.exchange_info,
        value.market_data.depth,
        // sapi
        value.sapi.listen_key,
        value.sapi.isolated_margin_user_data_stream,
        value.sapi.cross_account,
        value.sapi.margin_user_data_stream,
        value.sapi.margin_open_orders,
        value.sapi.margin_order,
        // papi
        value.papi.ping_path,
        value.papi.listen_key,
        value.papi.balance,
        value.papi.margin_open_orders,
        value.papi.margin_my_trades,
        value.papi.margin_order,
        value.papi.margin_all_open_orders,
        //
        value.margin_side_effect_type);
  }
};
