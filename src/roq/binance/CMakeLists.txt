set(TARGET_NAME "${PROJECT_NAME}")

# autogen script

set(AUTOGEN_PY "${CMAKE_SOURCE_DIR}/autogen/autogen.py")

####

file(GLOB SCHEMAS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/*.json")

# h

foreach(schema ${SCHEMAS})
  get_filename_component(name ${schema} NAME_WE)
  set(HEADER "${CMAKE_CURRENT_BINARY_DIR}/${name}.h")
  add_custom_command(
    OUTPUT "${HEADER}"
    COMMAND ${PYTHON_EXECUTABLE} "${AUTOGEN_PY}" --namespace "roq/binance" --output-type state --file-type h "${schema}" > "${HEADER}"
    VERBATIM
    DEPENDS "${PYTHON_EXECUTABLE}" "${AUTOGEN_PY}" "${schema}")
  list(APPEND AUTOGEN_HEADERS "${HEADER}")
endforeach()

add_custom_target(autogen-headers ALL
  DEPENDS ${AUTOGEN_HEADERS})

# cpp

foreach(schema ${SCHEMAS})
  get_filename_component(name ${schema} NAME_WE)
  set(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp")
  add_custom_command(
    OUTPUT "${SOURCE}"
    COMMAND ${PYTHON_EXECUTABLE} "${AUTOGEN_PY}" --namespace "roq/binance" --output-type state --file-type cpp "${schema}" > "${SOURCE}"
    VERBATIM
    DEPENDS "${PYTHON_EXECUTABLE}" "${AUTOGEN_PY}" "${schema}")
  list(APPEND AUTOGEN_SOURCES "${SOURCE}")
endforeach()

add_custom_target(autogen-sources ALL
  DEPENDS ${AUTOGEN_SOURCES})

####

file(GLOB SCHEMAS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/json/*.json")

add_custom_target(ensure-json-directory-exists ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/json")

# h

foreach(schema ${SCHEMAS})
  get_filename_component(name ${schema} NAME_WE)
  set(HEADER "${CMAKE_CURRENT_BINARY_DIR}/json/${name}.h")
  add_custom_command(
    OUTPUT "${HEADER}"
    COMMAND ${PYTHON_EXECUTABLE} "${AUTOGEN_PY}" --namespace "roq/binance/json" --output-type json --file-type h "${schema}" > "${HEADER}"
    VERBATIM
    DEPENDS "${PYTHON_EXECUTABLE}" "${AUTOGEN_PY}" "${schema}" ensure-json-directory-exists)
  list(APPEND AUTOGEN_JSON_HEADERS "${HEADER}")
endforeach()

add_custom_target(autogen-json-headers ALL
  DEPENDS ${AUTOGEN_JSON_HEADERS})

# cpp

foreach(schema ${SCHEMAS})
  get_filename_component(name ${schema} NAME_WE)
  set(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/json/${name}.cpp")
  add_custom_command(
    OUTPUT "${SOURCE}"
    COMMAND ${PYTHON_EXECUTABLE} "${AUTOGEN_PY}" --namespace "roq/binance/json" --output-type json --file-type cpp "${schema}" > "${SOURCE}"
    VERBATIM
    DEPENDS "${PYTHON_EXECUTABLE}" "${AUTOGEN_PY}" "${schema}" ensure-json-directory-exists)
  list(APPEND AUTOGEN_JSON_SOURCES "${SOURCE}")
endforeach()

add_custom_target(autogen-json-sources ALL
  DEPENDS ${AUTOGEN_JSON_SOURCES})

####

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
