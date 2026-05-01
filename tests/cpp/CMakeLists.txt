cmake_minimum_required(VERSION 3.20)
project(subzero_protocol_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

include(FetchContent)

FetchContent_Declare(
  ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ArduinoJson)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
  GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

set(PROTOCOL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../components/subzero_protocol)
set(APPLIANCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../components/subzero_appliance)
set(FIXTURES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures)

add_library(subzero_protocol STATIC
  ${PROTOCOL_DIR}/protocol.cpp
)
target_include_directories(subzero_protocol PUBLIC ${PROTOCOL_DIR})
target_link_libraries(subzero_protocol PUBLIC ArduinoJson)

# Hub library — depends on subzero_protocol for buffer/commands/log_sanitize.
add_library(subzero_appliance STATIC
  ${APPLIANCE_DIR}/hub.cpp
)
target_include_directories(subzero_appliance PUBLIC ${APPLIANCE_DIR})
target_link_libraries(subzero_appliance PUBLIC subzero_protocol)

enable_testing()

add_executable(protocol_tests
  protocol_test.cpp
  buffer_test.cpp
  sanitize_test.cpp
  commands_test.cpp
  gatt_handles_test.cpp
  dispatch_test.cpp
  hub_test.cpp
  lifecycle_guards_test.cpp
)
target_link_libraries(protocol_tests PRIVATE
  subzero_protocol
  subzero_appliance
  nlohmann_json::nlohmann_json
  GTest::gtest_main
)
target_compile_definitions(protocol_tests PRIVATE
  FIXTURES_DIR="${FIXTURES_DIR}"
  PROTOCOL_DIR="${PROTOCOL_DIR}"
  APPLIANCE_DIR="${APPLIANCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(protocol_tests)
