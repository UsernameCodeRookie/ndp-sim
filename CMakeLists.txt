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

# --- Precision demo example ---
add_executable(precision_demo src/examples/precision_demo.cpp)
target_include_directories(precision_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- DRAMsim3 library ---
add_subdirectory(third_party/DRAMsim3)

# --- GoogleTest configuration ---
add_subdirectory(third_party/googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})


# --- GEMM Operator tests ---
add_executable(gemm_operator_tests tests/test_gemm_operator.cpp)
target_include_directories(gemm_operator_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gemm_operator_tests gtest gtest_main pthread)
add_test(NAME gemm_operator_tests COMMAND gemm_operator_tests)

# --- MAC PE tests ---
add_executable(test_mac_pe tests/test_mac_pe.cpp)
target_include_directories(test_mac_pe PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- ALU tests ---
add_executable(test_alu tests/test_alu.cpp)
target_include_directories(test_alu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_alu gtest gtest_main pthread)
add_test(NAME test_alu COMMAND test_alu)

# --- PE tests ---
add_executable(test_pe tests/test_pe.cpp)
target_include_directories(test_pe PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_pe gtest gtest_main pthread)
add_test(NAME test_pe COMMAND test_pe)

# --- PE Dual ALU tests ---
add_executable(test_pe_dual_alu tests/test_pe_dual_alu.cpp)
target_include_directories(test_pe_dual_alu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_pe_dual_alu gtest gtest_main pthread)
add_test(NAME test_pe_dual_alu COMMAND test_pe_dual_alu)

# --- Pipeline tests ---
add_executable(test_pipeline tests/test_pipeline.cpp)
target_include_directories(test_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_pipeline gtest gtest_main pthread)
add_test(NAME test_pipeline COMMAND test_pipeline)

# --- LSU tests ---
add_executable(test_lsu tests/test_lsu.cpp)
target_include_directories(test_lsu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_lsu gtest gtest_main pthread)
add_test(NAME test_lsu COMMAND test_lsu)

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
