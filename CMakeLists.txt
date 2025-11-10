cmake_minimum_required(VERSION 3.15)

# Project name and languages
project(NDP_Simulator LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O2 -pipe)
elseif (MSVC)
    add_compile_options(/W4 /WX /permissive-)
endif()

# --- Event-driven framework example ---
add_executable(event_example src/event_example.cpp)
target_include_directories(event_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- GoogleTest configuration ---
if(WIN32)
    add_subdirectory(third_party/googletest)
else()
    find_package(GTest REQUIRED)
endif()
include_directories(${GTEST_INCLUDE_DIRS})


# --- Event-driven framework tests ---
add_executable(event_tests tests/test_event.cpp)
target_include_directories(event_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(event_tests GTest::gtest GTest::gtest_main)
add_test(NAME event_tests COMMAND event_tests)

enable_testing()

# --- Enable debug mode ---
# target_compile_definitions(pe_tests PRIVATE ENABLE_DEBUG)

# -----------------------------
# Optional: Python module using pybind11
# -----------------------------
# option(BUILD_PYTHON_MODULE "Build Python module with pybind11" OFF)

# if(BUILD_PYTHON_MODULE)
#     # Find pybind11
#     find_package(pybind11 REQUIRED)

#     # Create Python module target
#     pybind11_add_module(ndp_sim_py src/bindings.cpp)
#     target_include_directories(ndp_sim_py PRIVATE ${CMAKE_SOURCE_DIR}/src)

#     message(STATUS "Python module ndp_sim_py enabled")
# endif()
