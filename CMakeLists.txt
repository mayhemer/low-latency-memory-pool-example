cmake_minimum_required(VERSION 3.20)
project(pool.test VERSION 0.1.0 LANGUAGES C CXX)
include(FetchContent)

add_executable(pool.test src/pool.test.cpp)

set_property(TARGET pool.test PROPERTY CXX_STANDARD 23)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(pool.test PRIVATE -Wall -Wextra -Wpedantic)
endif()

FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main)

FetchContent_Declare(googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG main) # need main for benchmark::benchmark

FetchContent_MakeAvailable(
        googletest
        googlebenchmark)

target_link_libraries(pool.test benchmark::benchmark)

