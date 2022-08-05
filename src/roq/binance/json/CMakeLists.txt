set(TARGET_NAME "${PROJECT_NAME}-json")

set(SOURCES market_stream_parser.cpp user_stream_parser.cpp utils.cpp)

include(RoqAutogen)

set(AUTOGEN_SCHEMAS
    account.json
    agg_trade.json
    balance.json
    balances_item.json
    balance_update.json
    book_ticker.json
    cancel_all_open_orders.json
    cancel_order.json
    cancel_replace_order.json
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
    result.json
    side.json
    stream.json
    symbol.json
    symbol_status.json
    ticker.json
    time_in_force.json
    trade.json
    user_stream.json)

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

roq_gitignore(OUTPUT ".gitignore" SOURCES "${TARGET_NAME}" ${AUTOGEN_HEADERS} ${AUTOGEN_SOURCES})

add_library("${TARGET_NAME}" STATIC ${SOURCES} ${AUTOGEN_SOURCES})

add_dependencies("${TARGET_NAME}" autogen-json-headers)

target_link_libraries("${TARGET_NAME}" roq-core::roq-core roq-logging::roq-logging fmt::fmt)
