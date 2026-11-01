cmake_minimum_required(VERSION 3.16)
project(lms VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(LMS_COVERAGE "Build with gcov coverage instrumentation" OFF)
option(LMS_BUILD_TESTS "Build the automated tests" ON)

find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

add_library(lmscore
  src/common.cpp src/domain.cpp src/schema.cpp src/data.cpp src/security.cpp src/services.cpp)
target_include_directories(lmscore PUBLIC src)
target_link_libraries(lmscore PUBLIC SQLite::SQLite3 PkgConfig::SODIUM)
target_compile_options(lmscore PRIVATE -Wall -Wextra -Wpedantic)
if(LMS_COVERAGE)
  target_compile_options(lmscore PUBLIC --coverage -O0 -g)
  target_link_options(lmscore PUBLIC --coverage)
endif()

install(TARGETS lmscore ARCHIVE DESTINATION lib)

add_library(lmsui src/ui.cpp)
target_link_libraries(lmsui PUBLIC lmscore)
target_compile_options(lmsui PRIVATE -Wall -Wextra -Wpedantic)

add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lmsui)
target_compile_options(lms PRIVATE -Wall -Wextra -Wpedantic)
install(TARGETS lms RUNTIME DESTINATION bin)

if(LMS_BUILD_TESTS)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz)
    FetchContent_MakeAvailable(googletest)
  endif()
  add_executable(lms_tests
    tests/test_penalty_domain.cpp tests/test_data.cpp tests/test_auth_admin.cpp
    tests/test_submission_marking.cpp tests/test_moderation.cpp tests/test_robustness.cpp
    tests/test_performance.cpp tests/test_ui.cpp tests/test_system_usecases.cpp tests/test_cr1_class_extension.cpp tests/test_concurrency.cpp)
  find_package(Threads REQUIRED)
  target_link_libraries(lms_tests PRIVATE lmsui GTest::gtest_main Threads::Threads)
  include(GoogleTest)
  gtest_discover_tests(lms_tests DISCOVERY_TIMEOUT 120)
endif()
