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

# --- DRAMsim3 library ---
add_subdirectory(third_party/DRAMsim3)

# --- GoogleTest configuration ---
add_subdirectory(third_party/googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

# --- ALU tests ---
add_executable(test_alu tests/test_alu.cpp)
target_include_directories(test_alu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_alu gtest gtest_main pthread)
add_test(NAME test_alu COMMAND test_alu)

# --- FPU tests ---
add_executable(test_fpu tests/test_fpu.cpp)
target_include_directories(test_fpu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_fpu gtest gtest_main pthread)
add_test(NAME test_fpu COMMAND test_fpu)

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

# --- PingPong Connection tests ---
add_executable(test_pingpong_connection tests/test_pingpong_connection.cpp)
target_include_directories(test_pingpong_connection PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_pingpong_connection gtest gtest_main pthread)
add_test(NAME test_pingpong_connection COMMAND test_pingpong_connection)

# --- Credit Connection tests ---
add_executable(test_credit_connection tests/test_credit_connection.cpp)
target_include_directories(test_credit_connection PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_credit_connection gtest gtest_main pthread)
add_test(NAME test_credit_connection COMMAND test_credit_connection)

# --- MLU tests ---
add_executable(test_mlu tests/test_mlu.cpp)
target_include_directories(test_mlu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_mlu gtest gtest_main pthread)
add_test(NAME test_mlu COMMAND test_mlu)

# --- DVU tests ---
add_executable(test_dvu tests/test_dvu.cpp)
target_include_directories(test_dvu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_dvu gtest gtest_main pthread)
add_test(NAME test_dvu COMMAND test_dvu)

# --- BRU tests ---
add_executable(test_bru tests/test_bru.cpp)
target_include_directories(test_bru PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_bru gtest gtest_main pthread)
add_test(NAME test_bru COMMAND test_bru)

# --- Regfile tests ---
add_executable(test_regfile tests/test_regfile.cpp)
target_include_directories(test_regfile PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_regfile gtest gtest_main pthread)
add_test(NAME test_regfile COMMAND test_regfile)

# --- SCore tests ---
add_executable(test_core tests/test_core.cpp)
target_include_directories(test_core PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_core gtest gtest_main pthread)
add_test(NAME test_core COMMAND test_core)

# --- SCore Dispatch tests ---
add_executable(test_core_dispatch tests/test_core_dispatch.cpp)
target_include_directories(test_core_dispatch PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_core_dispatch gtest gtest_main pthread)
add_test(NAME test_core_dispatch COMMAND test_core_dispatch)

# --- SCore Event-Driven tests ---
add_executable(test_core_eventdriven tests/test_core_eventdriven.cpp)
target_include_directories(test_core_eventdriven PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_core_eventdriven gtest gtest_main pthread)
add_test(NAME test_core_eventdriven COMMAND test_core_eventdriven)

# --- RVV Vector ALU tests ---
add_executable(test_rvv_alu tests/test_rvv_alu.cpp)
target_include_directories(test_rvv_alu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_rvv_alu gtest gtest_main pthread)
add_test(NAME test_rvv_alu COMMAND test_rvv_alu)

# --- RVV Vector DVU tests ---
add_executable(test_rvv_dvu tests/test_rvv_dvu.cpp)
target_include_directories(test_rvv_dvu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_rvv_dvu gtest gtest_main pthread)
add_test(NAME test_rvv_dvu COMMAND test_rvv_dvu)

# --- MAC Example ---
add_executable(mac_example example/mac_example.cpp)
target_include_directories(mac_example PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mac_example pthread)

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
