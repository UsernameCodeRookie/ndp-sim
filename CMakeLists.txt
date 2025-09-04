cmake_minimum_required(VERSION 3.15)

project(NDP_Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O2 -pipe)
elseif (MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# --- Main executable ---
add_executable(ndp_sim src/main.cpp)
target_include_directories(ndp_sim PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- Unit tests with system-installed GoogleTest ---
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(pe_tests tests/test_pe.cpp)
target_include_directories(pe_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pe_tests GTest::gtest GTest::gtest_main)

enable_testing()
add_test(NAME pe_tests COMMAND pe_tests)
