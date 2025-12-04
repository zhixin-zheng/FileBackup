cmake_minimum_required(VERSION 3.14)
project(FileBackup)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

message(STATUS "正在配置 GoogleTest (将自动下载)...")

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(googletest)

message(STATUS "GoogleTest 配置完成")


include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB CORE_SOURCES "src/*.cpp")

add_library(backup_core SHARED ${CORE_SOURCES})


enable_testing()

add_executable(run_unit_tests 
    src/traverser.cpp          # <--- 包含你的实现文件
    tests/test_traverser.cpp   # <--- 包含上面的测试代码
)

target_include_directories(run_unit_tests PRIVATE include)

target_link_libraries(run_unit_tests 
    PRIVATE 
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(run_unit_tests)