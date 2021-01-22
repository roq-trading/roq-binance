set(TARGET_NAME "${PROJECT_NAME}")

set(SOURCES
    application.cpp
    config.cpp
    gateway.cpp
    main.cpp
    market_stream.cpp
    options.cpp
    random.cpp
    rest.cpp
    user_stream.cpp)

add_subdirectory("json")

include(RoqAutogen)

set(AUTOGEN_SCHEMAS rest_state.json)

roq_autogen(
  OUTPUT
  AUTOGEN_HEADERS
  NAMESPACE
  "roq/binance"
  OUTPUT_TYPE
  "state"
  FILE_TYPE
  "h"
  SOURCES
  ${AUTOGEN_SCHEMAS})

add_custom_target(autogen-headers ALL DEPENDS ${AUTOGEN_HEADERS})

roq_autogen(
  OUTPUT
  AUTOGEN_SOURCES
  NAMESPACE
  "roq/binance"
  OUTPUT_TYPE
  "state"
  FILE_TYPE
  "cpp"
  SOURCES
  ${AUTOGEN_SCHEMAS})

add_executable("${TARGET_NAME}" ${SOURCES} ${AUTOGEN_SOURCES})

add_dependencies("${TARGET_NAME}" autogen-headers autogen-json-headers)

target_link_libraries(
  "${TARGET_NAME}"
  "${PROJECT_NAME}-json"
  roq-server::roq-server
  roq-core::roq-core
  roq-logging::roq-logging
  absl::flags
  absl::flat_hash_map
  absl::flat_hash_set
  ${UUID_LIBRARIES}
  ${RT_LIBRARIES})

set_target_properties("${TARGET_NAME}" PROPERTIES LINK_FLAGS_RELEASE -s)

target_compile_definitions(
  "${TARGET_NAME}"
  PRIVATE ROQ_PACKAGE_NAME="${TARGET_NAME}" ROQ_BUILD_VERSION="${GIT_REPO_VERSION}"
          ROQ_GIT_DESCRIBE_HASH="${GIT_DESCRIBE_HASH}" ROQ_BUILD_TYPE="${ROQ_BUILD_TYPE}")

install(TARGETS ${TARGET_NAME})
