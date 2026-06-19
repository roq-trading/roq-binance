set(TARGET_NAME ${PROJECT_NAME}-protocol-json)

set(SOURCES
    encoder.cpp
    map.cpp
    market_stream_parser.cpp
    user_stream_parser.cpp
    utils.cpp
    wsapi_parser.cpp
    wsapi_request.cpp)

include(RoqAutogen)

set(AUTOGEN_SCHEMAS
    account_ack.json
    agg_trade.json
    agg_trade_data.json
    amended_order.json
    balance.json
    balance2.json
    balance_update.json
    balance_update_data.json
    balances_ack.json
    balances_item.json
    book_ticker.json
    book_ticker_data.json
    cancel_all_open_orders_ack.json
    cancel_order_ack.json
    cancel_replace_mode.json
    cancel_replace_order_ack.json
    cancel_replace_order_error.json
    cancel_restrictions.json
    cross_margin_account.json
    depth.json
    depth_ack.json
    depth_data.json
    depth_item.json
    depth_update.json
    depth_update_data.json
    error.json
    error_error.json
    event_type.json
    exchange_filter.json
    exchange_info_ack.json
    execution_report.json
    execution_report_data.json
    execution_type.json
    field.json
    fill.json
    filter.json
    filter_type.json
    filters.json
    interval.json
    list_status.json
    list_status_data.json
    list_status_item.json
    listen_key_ack.json
    listen_key_ack_margin.json
    mini_ticker.json
    mini_ticker_data.json
    new_order_ack.json
    open_orders_ack.json
    order.json
    order_amend_keep_priority.json
    order_status.json
    order_type.json
    outbound_account_position.json
    outbound_account_position_data.json
    rate_limit_type.json
    rate_limits_item.json
    result.json
    self_trade_prevention_mode.json
    server_shutdown.json
    server_shutdown_data.json
    side.json
    stream.json
    success_or_failure.json
    symbol.json
    symbol_status.json
    ticker.json
    time_in_force.json
    trade.json
    trade2.json
    trade_data.json
    trades_ack.json
    user_asset.json
    user_stream.json
    wsapi_account.json
    wsapi_balance_update.json
    wsapi_cancel_open_orders.json
    wsapi_cancel_order.json
    wsapi_event_stream_terminated.json
    wsapi_event_stream_terminated_event.json
    wsapi_execution_report.json
    wsapi_open_orders.json
    wsapi_order_amend_keep_priority.json
    wsapi_order_place.json
    wsapi_outbound_account_position.json
    wsapi_rate_limits_item.json
    wsapi_session_logon.json
    wsapi_session_logon_result.json
    wsapi_trades.json
    wsapi_type.json
    wsapi_user_data_stream_subscribe.json
    wsapi_user_data_stream_subscribe_result.json)

roq_autogen_hpp(
  OUTPUT
  AUTOGEN_HEADERS
  SOURCES
  ${AUTOGEN_SCHEMAS}
  TEMPLATE_DIR
  ${TEMPLATE_DIR}
  TEMPLATE_TYPE
  "json")

add_custom_target(protocol-json-autogen-headers ALL DEPENDS ${AUTOGEN_HEADERS})

roq_autogen_cpp(
  OUTPUT
  AUTOGEN_SOURCES
  SOURCES
  ${AUTOGEN_SCHEMAS}
  TEMPLATE_DIR
  ${TEMPLATE_DIR}
  TEMPLATE_TYPE
  "json")

roq_gitignore(OUTPUT .gitignore SOURCES ${TARGET_NAME} ${AUTOGEN_HEADERS} ${AUTOGEN_SOURCES})

add_library(${TARGET_NAME} OBJECT ${SOURCES} ${AUTOGEN_SOURCES})

add_dependencies(${TARGET_NAME} protocol-json-autogen-headers)

target_link_libraries(${TARGET_NAME} PRIVATE roq-core::roq-core roq-codec::roq-codec roq-utils::roq-utils roq-logging::roq-logging fmt::fmt)
