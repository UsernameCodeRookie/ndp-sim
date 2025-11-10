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
add_executable(event_example src/examples/event_examples.cpp)
target_include_directories(event_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- Architecture framework example ---
add_executable(architecture_example src/examples/architecture_examples.cpp)
target_include_directories(architecture_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- Pipeline example ---
add_executable(pipeline_example src/examples/pipeline_examples.cpp)
target_include_directories(pipeline_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- PE (Processing Element) example ---
add_executable(pe_example src/examples/pe_examples.cpp)
target_include_directories(pe_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- Architecture connection debug ---
add_executable(connection_debug tests/test_connection_debug.cpp)
target_include_directories(connection_debug PRIVATE ${CMAKE_SOURCE_DIR}/src)

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

# --- Architecture framework tests ---
add_executable(architecture_tests tests/test_architecture.cpp)
target_include_directories(architecture_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(architecture_tests GTest::gtest GTest::gtest_main)
add_test(NAME architecture_tests COMMAND architecture_tests)

# --- Pipeline tests ---
add_executable(pipeline_tests tests/test_pipeline.cpp)
target_include_directories(pipeline_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pipeline_tests GTest::gtest GTest::gtest_main)
add_test(NAME pipeline_tests COMMAND pipeline_tests)

# --- PE tests ---
add_executable(pe_tests tests/test_pe.cpp)
target_include_directories(pe_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pe_tests GTest::gtest GTest::gtest_main)
add_test(NAME pe_tests COMMAND pe_tests)

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
