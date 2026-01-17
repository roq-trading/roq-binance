include(RoqAutogen)

set(TARGET_NAME ${PROJECT_NAME}-test)

set(SOURCES
    json_account_ack.cpp
    json_agg_trade.cpp
    json_book_ticker.cpp
    json_cancel_all_open_orders_ack.cpp
    json_cancel_order_ack.cpp
    json_cancel_replace_order_ack.cpp
    json_depth_update.cpp
    json_encoder.cpp
    json_error.cpp
    json_exchange_info_ack.cpp
    json_execution_report.cpp
    json_filters.cpp
    json_listen_key_ack.cpp
    json_mini_ticker.cpp
    json_new_order_ack.cpp
    json_open_orders_ack.cpp
    json_outbound_account_position.cpp
    json_result.cpp
    json_session_logon.cpp
    json_wsapi_account.cpp
    json_wsapi_cancel_open_orders.cpp
    json_wsapi_cancel_order.cpp
    json_wsapi_execution_report.cpp
    json_wsapi_open_orders.cpp
    json_wsapi_order_place.cpp
    json_wsapi_outbound_account_position.cpp
    json_wsapi_session_logon.cpp
    json_wsapi_trades.cpp
    json_wsapi_user_data_stream_subscribe.cpp
    tools_crypto.cpp
    main.cpp)

roq_gitignore(OUTPUT .gitignore SOURCES ${TARGET_NAME})

add_executable(${TARGET_NAME} ${SOURCES})

target_link_libraries(${TARGET_NAME} PRIVATE ${PROJECT_NAME}-json ${PROJECT_NAME}-tools Catch2::Catch2)

if(ROQ_BUILD_TYPE STREQUAL "Release")
  set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)
endif()

add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
