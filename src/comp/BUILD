package(default_visibility = ["//visibility:public"])

# Component libraries
cc_library(
    name = "core",
    hdrs = glob(["core/**/*.h"]),
    includes = ["core"],
)

cc_library(
    name = "dram",
    hdrs = ["dram.h"],
)

cc_library(
    name = "precision",
    hdrs = ["precision.h"],
)

cc_library(
    name = "rvv",
    hdrs = glob(["rvv/**/*.h"]),
    includes = ["rvv"],
)
