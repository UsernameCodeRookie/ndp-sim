cmake_minimum_required(VERSION 3.15)

# Project name and languages
project(NDP_Simulator LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O2 -pipe)
elseif (MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# --- GEMM 64x64 example ---
add_executable(gemm_64x64_example src/examples/gemm_64x64_example.cpp)
target_include_directories(gemm_64x64_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- GEMM Operator with TPU example ---
add_executable(gemm_operator_tpu_example src/examples/gemm_operator_tpu_example.cpp)
target_include_directories(gemm_operator_tpu_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- DRAMsim3 library ---
add_subdirectory(third_party/DRAMsim3)

# --- GoogleTest configuration ---
if(WIN32)
    add_subdirectory(third_party/googletest)
else()
    find_package(GTest REQUIRED)
endif()
include_directories(${GTEST_INCLUDE_DIRS})


# --- GEMM Operator tests ---
add_executable(gemm_operator_tests tests/test_gemm_operator.cpp)
target_include_directories(gemm_operator_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gemm_operator_tests GTest::gtest GTest::gtest_main)
add_test(NAME gemm_operator_tests COMMAND gemm_operator_tests)

enable_testing()

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
