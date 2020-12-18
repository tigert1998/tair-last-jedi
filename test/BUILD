cc_library(
    name = "utils",
    hdrs = ["utils.h"],
)

cc_test(
    name = "correctness_test",
    srcs = ["correctness_test.cc"],
    deps = [
        "//common:db_header",
        "//engine:engine",
        ":utils",
        "@googletest//:gtest_main",
    ],
    linkopts = [
        "-L/usr/local/lib",
        "-lpmem",
    ],
    copts = ["-DLOCAL_DEBUG"],
)

cc_test(
    name = "persistence_test",
    srcs = ["persistence_test.cc"],
    deps = [
        "//common:db_header",
        "//engine:engine",
        ":utils",
        "@googletest//:gtest_main",
    ],
    linkopts = [
        "-L/usr/local/lib",
        "-lpmem",
    ],
    copts = ["-DLOCAL_DEBUG"],
)