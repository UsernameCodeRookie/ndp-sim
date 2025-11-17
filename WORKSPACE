workspace(name = "ndp_sim")

# Bazel dependency: Google Test
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "com_google_googletest",
    urls = ["https://github.com/google/googletest/archive/v1.14.0.zip"],
    strip_prefix = "googletest-1.14.0",
    sha256 = "94c634d499558a76fa649edb13721dce6e980ab6fcf72fc3f9cab9b67d39e712",
)

# Load rules_cc for C++ support
http_archive(
    name = "rules_cc",
    urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.9/rules_cc-0.0.9.tar.gz"],
    sha256 = "2ac8e3f3a7a5e29fcd7e6d24330b51290528e435f51ea209ba1d44bef265651e",
    strip_prefix = "rules_cc-0.0.9",
)

load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")
rules_cc_dependencies()

# Local DRAMsim3 dependency (if needed to be integrated)
local_repository(
    name = "dramsim3",
    path = "third_party/DRAMsim3",
)
