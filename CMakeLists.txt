cmake_minimum_required(VERSION 3.20)
project(pool.test VERSION 0.1.0 LANGUAGES C CXX)

add_executable(pool.test src/pool.test.cpp)

set_property(TARGET pool.test PROPERTY CXX_STANDARD 23)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(pool.test PRIVATE -Wall -Wextra -Wpedantic)
endif()
