set(TARGET_NAME ${PROJECT_NAME}-json)

set(SOURCES market_stream_parser.cpp user_stream_parser.cpp utils.cpp wsapi_parser_2.cpp wsapi_request.cpp)

include(RoqAutogen)

set(AUTOGEN_SCHEMAS
    account.json
    agg_trade.json
    balance2.json
    balance.json
    balances.json
    balances_item.json
    balance_update.json
    book_ticker.json
    cancel_all_open_orders.json
    cancel_order.json
    cancel_replace_mode.json
    cancel_replace_order_error.json
    cancel_replace_order.json
    cancel_restrictions.json
    depth_item.json
    depth.json
    depth_update.json
    error.json
    event_type.json
    exchange_filter.json
    exchange_info.json
    execution_report.json
    execution_type.json
    field.json
    fill.json
    filter.json
    filters.json
    filter_type.json
    interval.json
    listen_key.json
    list_status_item.json
    list_status.json
    mini_ticker.json
    new_order.json
    open_orders.json
    order.json
    order_status.json
    order_type.json
    outbound_account_position.json
    rate_limits_item.json
    rate_limit_type.json
    result.json
    self_trade_prevention_mode.json
    side.json
    stream.json
    success_or_failure.json
    symbol.json
    symbol_status.json
    ticker.json
    time_in_force.json
    trade2.json
    trade.json
    trades.json
    user_stream.json
    wsapi_account.json
    wsapi_cancel_open_orders.json
    wsapi_cancel_order.json
    wsapi_cancel_replace_order.json
    wsapi_listen_key.json
    wsapi_open_orders.json
    wsapi_order_place.json
    wsapi_rate_limits_item.json
    wsapi_trades.json
    wsapi_type.json)

roq_autogen(
  OUTPUT
  AUTOGEN_HEADERS
  NAMESPACE
  "roq/binance/json"
  OUTPUT_TYPE
  "json"
  FILE_TYPE
  "hpp"
  SOURCES
  ${AUTOGEN_SCHEMAS})

add_custom_target(autogen-json-headers ALL DEPENDS ${AUTOGEN_HEADERS})

roq_autogen(
  OUTPUT
  AUTOGEN_SOURCES
  NAMESPACE
  "roq/binance/json"
  OUTPUT_TYPE
  "json"
  FILE_TYPE
  "cpp"
  SOURCES
  ${AUTOGEN_SCHEMAS})

roq_gitignore(OUTPUT .gitignore SOURCES ${TARGET_NAME} ${AUTOGEN_HEADERS} ${AUTOGEN_SOURCES})

add_library(${TARGET_NAME} OBJECT ${SOURCES} ${AUTOGEN_SOURCES})

add_dependencies(${TARGET_NAME} autogen-json-headers)

target_link_libraries(${TARGET_NAME} roq-core::roq-core roq-utils::roq-utils roq-logging::roq-logging fmt::fmt)
