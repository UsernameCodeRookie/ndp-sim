# Main BUILD file for ndp-sim project
package(default_visibility = ["//visibility:public"])

# C++ headers library for core components
cc_library(
    name = "ndp_core",
    hdrs = [
        "src/architecture.h",
        "src/buffer.h",
        "src/component.h",
        "src/connection.h",
        "src/event.h",
        "src/packet.h",
        "src/pipeline.h",
        "src/port.h",
        "src/scheduler.h",
        "src/stage.h",
        "src/tick.h",
        "src/trace.h",
    ],
    includes = ["src"],
    deps = [
        "//src/comp:core",
        "//src/comp:dram",
        "//src/comp:precision",
        "//src/comp:rvv",
        "//src/conn:credit",
        "//src/conn:link",
        "//src/conn:ready_valid",
        "//src/conn:regf_wire",
        "//src/conn:wire",
    ],
)

# Main simulator executable
cc_binary(
    name = "main",
    srcs = ["src/npu.cpp"],
    copts = [
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-O2",
        "-pipe",
        "-std=c++20",
    ],
    includes = ["src"],
    deps = [":ndp_core"],
)
