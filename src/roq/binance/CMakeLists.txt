set(TARGET_NAME "${PROJECT_NAME}")

include(RoqAutogen)

roq_autogen(
  OUTPUT AUTOGEN_HEADERS
  PATTERN "*.json"
  NAMESPACE "roq/binance"
  OUTPUT_TYPE "state"
  FILE_TYPE "h")

add_custom_target(autogen-headers ALL
  DEPENDS ${AUTOGEN_HEADERS})


roq_autogen(
  OUTPUT AUTOGEN_SOURCES
  PATTERN "*.json"
  NAMESPACE "roq/binance"
  OUTPUT_TYPE "state"
  FILE_TYPE "cpp")

add_custom_target(autogen-sources ALL
  DEPENDS ${AUTOGEN_SOURCES})

# json

add_custom_target(ensure-json-directory-exists ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/json")

roq_autogen(
  OUTPUT AUTOGEN_JSON_HEADERS
  PATTERN "json/*.json"
  NAMESPACE "roq/binance/json"
  OUTPUT_TYPE "json"
  FILE_TYPE "h"
  DEPENDS ensure-json-directory-exists)

add_custom_target(autogen-json-headers ALL
  DEPENDS ${AUTOGEN_JSON_HEADERS})

roq_autogen(
  OUTPUT AUTOGEN_JSON_SOURCES
  PATTERN "json/*.json"
  NAMESPACE "roq/binance/json"
  OUTPUT_TYPE "json"
  FILE_TYPE "cpp"
  DEPENDS ensure-json-directory-exists)

add_custom_target(autogen-json-sources ALL
  DEPENDS ${AUTOGEN_JSON_SOURCES})

# main

set(SOURCES
  application.cpp
  config.cpp
  gateway.cpp
  json/market_stream_parser.cpp
  json/user_stream_parser.cpp
  json/utils.cpp
  main.cpp
  market_stream.cpp
  options.cpp
  random.cpp
  rest.cpp
  user_stream.cpp
)

add_executable("${TARGET_NAME}"
  ${SOURCES}
  ${AUTOGEN_SOURCES}
  ${AUTOGEN_JSON_SOURCES})

add_dependencies("${TARGET_NAME}"
  autogen-headers
  autogen-sources
  autogen-json-headers
  autogen-json-sources)

target_link_libraries("${TARGET_NAME}"
  roq-server
  roq-core
  roq-api::roq-api
  roq-logging
  absl::flat_hash_map
  absl::flat_hash_set
  gflags
  OpenSSL::SSL
  ${HTTP_PARSER_LIBRARIES}
  ${BROTLI_LIBRARIES}
  ${RT_LIBRARIES}
)

target_compile_definitions("${TARGET_NAME}" PRIVATE
  PACKAGE_NAME="${TARGET_NAME}"
  VERSION="${GIT_REPO_VERSION}")

install(TARGETS ${TARGET_NAME})
