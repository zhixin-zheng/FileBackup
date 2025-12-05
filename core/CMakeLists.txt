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
include(GoogleTest)

# 测试 遍历器 ------

add_executable(test_traverser tests/test_traverser.cpp)

target_link_libraries(test_traverser 
    PRIVATE 
    backup_core
    GTest::gtest_main
)

gtest_discover_tests(test_traverser)

# 测试 压缩器 -----

add_executable(test_compressor tests/test_compressor.cpp)

target_link_libraries(test_compressor 
    PRIVATE 
    backup_core 
    GTest::gtest_main
)

gtest_discover_tests(test_compressor)

# 测试 打包器 -----

add_executable(test_packer tests/test_packer.cpp)

target_link_libraries(test_packer 
    PRIVATE 
    backup_core 
    GTest::gtest_main
)

gtest_discover_tests(test_packer)