package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "all_files",
    srcs = glob(
        ["**/*"]
    ),
)

cc_library(
    name = "profile_io",
    hdrs = ["profile_io.h"],
    srcs = ["profile_io.cc"],
    deps = [
        "//profiling/util:endian",
    ],
)

cc_test(
    name = "profile_io_test",
    srcs = ["profile_io_test.cc"],
    copts = ["-fno-access-control"],
    data = ["cpu_profile_sample"],
    deps = [
        ":profile_io",
        "@com_google_googletest//:gtest_main",
    ],
)
