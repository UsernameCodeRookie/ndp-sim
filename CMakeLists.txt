cmake_minimum_required(VERSION 3.15)

project(NDP_Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O2 -pipe)
elseif (MSVC)
    add_compile_options(/W4 /WX /permissive-)
endif()

# --- Main executable ---
add_executable(ndp_sim src/main.cpp)
target_include_directories(ndp_sim PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- Find GoogleTest ---
if(WIN32)
    add_subdirectory(third_party/googletest)
else()
    find_package(GTest REQUIRED)
endif()
include_directories(${GTEST_INCLUDE_DIRS})

# --- PE tests ---
add_executable(pe_tests
    tests/test_pe.cpp
    tests/test_op_bar.cpp
    tests/test_transout.cpp
    tests/test_inner.cpp
    tests/test_outer.cpp
    tests/test_connect.cpp
)
target_include_directories(pe_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pe_tests GTest::gtest GTest::gtest_main)
add_test(NAME pe_tests COMMAND pe_tests)

enable_testing()

# --- Enable debug mode ---
target_compile_definitions(pe_tests PRIVATE ENABLE_DEBUG)

