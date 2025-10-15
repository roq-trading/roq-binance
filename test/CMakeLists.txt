include(RoqAutogen)

set(TARGET_NAME ${PROJECT_NAME}-test)

set(SOURCES
    json_account.cpp
    json_cancel_all_open_orders.cpp
    json_cancel_order.cpp
    json_cancel_replace_order.cpp
    json_depth_update.cpp
    json_exchange_info.cpp
    json_execution_report.cpp
    json_filters.cpp
    json_listen_key.cpp
    json_new_order.cpp
    json_open_orders.cpp
    json_outbound_account_position.cpp
    json_session_logon.cpp
    tools_crypto.cpp
    main.cpp)

roq_gitignore(OUTPUT .gitignore SOURCES ${TARGET_NAME})

add_executable(${TARGET_NAME} ${SOURCES})

target_link_libraries(${TARGET_NAME} PRIVATE ${PROJECT_NAME}-json ${PROJECT_NAME}-tools Catch2::Catch2)

if(ROQ_BUILD_TYPE STREQUAL "Release")
  set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)
endif()

add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
