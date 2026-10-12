cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
project(trdQueue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

add_library(trdqueue_core
  core/src/order_book.cpp
  core/src/trade_tape.cpp
  core/src/matching_engine.cpp
)
target_include_directories(trdqueue_core PUBLIC
  ${CMAKE_SOURCE_DIR}/core/include
)

target_compile_features(trdqueue_core PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(trdqueue_core PRIVATE /W4)
else()
  target_compile_options(trdqueue_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_OPEN_SSL OFF CACHE BOOL "" FORCE)
set(USE_MBED_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB ON CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  ixwebsocket
  URL https://github.com/machinezone/IXWebSocket/archive/refs/tags/v11.4.6.tar.gz
)
FetchContent_MakeAvailable(ixwebsocket)

add_executable(trdqueue_demo core/src/main.cpp)

add_executable(trdqueue_live
  ingest/live_main.cpp
  ingest/dashboard.cpp
  ingest/local_market.cpp
)
target_link_libraries(trdqueue_live PRIVATE trdqueue_core nlohmann_json::nlohmann_json ixwebsocket)
target_include_directories(trdqueue_live PRIVATE ${CMAKE_SOURCE_DIR}/ingest)
if(NOT MSVC)
  target_compile_options(trdqueue_live PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(trdqueue_demo PRIVATE
  ${CMAKE_SOURCE_DIR}/ingest
)
target_link_libraries(trdqueue_demo PRIVATE trdqueue_core nlohmann_json::nlohmann_json)
target_compile_definitions(trdqueue_demo PRIVATE
  TRDQUEUE_FIXTURE_PATH="${CMAKE_SOURCE_DIR}/core/fixtures/book_updates.json"
)
if(NOT MSVC)
  target_compile_options(trdqueue_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)


add_executable(trdqueue_tests
  core/tests/test_ring_buffer.cpp
  core/tests/test_order_book.cpp
  core/tests/test_trade_tape.cpp
  core/tests/test_decimal.cpp
  core/tests/test_matching_engine.cpp
  ingest/test_coinbase_parse.cpp
  ingest/test_book_view.cpp
  ingest/test_local_book.cpp
)
target_include_directories(trdqueue_tests PRIVATE ${CMAKE_SOURCE_DIR}/ingest)
target_link_libraries(trdqueue_tests PRIVATE trdqueue_core nlohmann_json::nlohmann_json GTest::gtest_main)
target_compile_definitions(trdqueue_tests PRIVATE
  TRDQUEUE_FIXTURE_PATH="${CMAKE_SOURCE_DIR}/core/fixtures/book_updates.json"
)
if(NOT MSVC)
  target_compile_options(trdqueue_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(trdqueue_tests)

