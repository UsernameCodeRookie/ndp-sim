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

# --- LSU (Load-Store Unit) example ---
add_executable(lsu_example src/examples/lsu_examples.cpp)
target_include_directories(lsu_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- TPU (Tensor Processing Unit) example ---
add_executable(tpu_example src/examples/tpu_examples.cpp)
target_include_directories(tpu_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- GEMM 64x64 example ---
add_executable(gemm_64x64_example src/examples/gemm_64x64_example.cpp)
target_include_directories(gemm_64x64_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- Software-Hardware Integration example ---
add_executable(sw_hw_integration_example src/examples/sw_hw_integration_example.cpp)
target_include_directories(sw_hw_integration_example PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- DRAMsim3 library ---
add_subdirectory(third_party/DRAMsim3)

# --- TPU with DRAMsim3 example ---
add_executable(tpu_dramsim3_example src/examples/tpu_dramsim3_examples.cpp)
target_include_directories(tpu_dramsim3_example PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/DRAMsim3/src
    ${CMAKE_SOURCE_DIR}/third_party/DRAMsim3/ext/headers
    ${CMAKE_SOURCE_DIR}/third_party/DRAMsim3/ext/fmt/include
)
target_link_libraries(tpu_dramsim3_example dramsim3)
target_compile_definitions(tpu_dramsim3_example PRIVATE FMT_HEADER_ONLY=1)

# Copy DRAMsim3 DLL to executable directory on Windows
if(WIN32)
    add_custom_command(TARGET tpu_dramsim3_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:dramsim3>
        $<TARGET_FILE_DIR:tpu_dramsim3_example>
        COMMENT "Copying DRAMsim3 DLL to executable directory"
    )
endif()

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

# --- LSU tests ---
add_executable(lsu_tests tests/test_lsu.cpp)
target_include_directories(lsu_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(lsu_tests GTest::gtest GTest::gtest_main)
add_test(NAME lsu_tests COMMAND lsu_tests)

# --- TPU tests ---
add_executable(tpu_tests tests/test_tpu.cpp)
target_include_directories(tpu_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpu_tests GTest::gtest GTest::gtest_main)
add_test(NAME tpu_tests COMMAND tpu_tests)

# --- GEMM Operator tests ---
add_executable(gemm_operator_tests tests/test_gemm_operator.cpp)
target_include_directories(gemm_operator_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gemm_operator_tests GTest::gtest GTest::gtest_main)
add_test(NAME gemm_operator_tests COMMAND gemm_operator_tests)

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
