package(default_visibility = ["//visibility:public"])

# Common test configuration
TEST_COPTS = [
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-O2",
    "-pipe",
    "-std=c++20",
]

TEST_LINKOPTS = ["-pthread"]

# ALU tests
cc_test(
    name = "test_alu",
    srcs = ["test_alu.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# FPU tests
cc_test(
    name = "test_fpu",
    srcs = ["test_fpu.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# Pipeline tests
cc_test(
    name = "test_pipeline",
    srcs = ["test_pipeline.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# LSU tests
cc_test(
    name = "test_lsu",
    srcs = ["test_lsu.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# Credit Connection tests
cc_test(
    name = "test_credit_connection",
    srcs = ["test_credit_connection.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# MLU tests
cc_test(
    name = "test_mlu",
    srcs = ["test_mlu.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# DVU tests
cc_test(
    name = "test_dvu",
    srcs = ["test_dvu.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# BRU tests
cc_test(
    name = "test_bru",
    srcs = ["test_bru.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# Regfile tests
cc_test(
    name = "test_regfile",
    srcs = ["test_regfile.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# SCore tests
cc_test(
    name = "test_core",
    srcs = ["test_core.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# SCore Dispatch tests
cc_test(
    name = "test_core_dispatch",
    srcs = ["test_core_dispatch.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# SCore Event-Driven tests
cc_test(
    name = "test_core_eventdriven",
    srcs = ["test_core_eventdriven.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# RVV Vector ALU tests
cc_test(
    name = "test_rvv_alu",
    srcs = ["test_rvv_alu.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# RVV Vector DVU tests
cc_test(
    name = "test_rvv_dvu",
    srcs = ["test_rvv_dvu.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# RVV Vector Register File tests
cc_test(
    name = "test_rvv_regfile",
    srcs = ["test_rvv_regfile.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# RVV Dispatch tests
cc_test(
    name = "test_rvv_dispatch",
    srcs = ["test_rvv_dispatch.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# RVV ROB tests
cc_test(
    name = "test_rvv_rob",
    srcs = ["test_rvv_rob.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# RVV Retire tests
cc_test(
    name = "test_rvv_retire",
    srcs = ["test_rvv_retire.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# RVV Backend integration tests
cc_test(
    name = "test_rvv_backend",
    srcs = ["test_rvv_backend.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# Core + RVV Integration tests
cc_test(
    name = "test_core_rvv_integration",
    srcs = ["test_core_rvv_integration.cpp"],
    copts = TEST_COPTS,
    linkopts = TEST_LINKOPTS,
    includes = ["../src"],
    deps = [
        "//:ndp_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
