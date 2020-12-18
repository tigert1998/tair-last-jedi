cc_library(
    name = "logger",
    srcs = ["logger.cc"],
    hdrs = ["logger.h"],
)

cc_library(
    name = "sync",
    hdrs = ["sync.h"],
    deps = [":tair_assert"]
)

cc_library(
    name = "tair_assert",
    hdrs = ["tair_assert.h"],
)

cc_library(
    name = "engine",
    srcs = [
        "engine.cc",
        "hash_index.cc",
        "pmem_allocator.cc",
        "record.cc",
        "subengine.cc"
    ],
    hdrs = [
        "engine.h",
        "config.h",
        "hash_index.h",
        "pmem_allocator.h",
        "record.h",
        "subengine.h",
        "utils.h"
    ],
    deps = [
        "//common:db_header",
        ":logger",
        ":tair_assert",
        "//common:cache_utils"
    ],
    visibility = ["//visibility:public"],
    copts = [
        "-DLOCAL_DEBUG"
    ]
)