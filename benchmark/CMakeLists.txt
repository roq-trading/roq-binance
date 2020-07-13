set(TARGET_NAME "${PROJECT_NAME}-benchmark")

set(SOURCES
  main.cpp
)

add_executable("${TARGET_NAME}"
  ${SOURCES}
)

target_link_libraries("${TARGET_NAME}"
  # ${PROJECT_NAME}
  benchmark::benchmark
)

install(TARGETS ${TARGET_NAME})
