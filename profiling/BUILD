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
    name = "cpu_profile",
    hdrs = ["cpu_profile.h"],
    srcs = ["cpu_profile.cc"],
    deps = [
        "//profiling/io:profile_io",
        "//profiling/symbol:profile_symbol",
        "@fmtlib//:fmtlib",
    ],
)

cc_test(
    name = "cpu_profile_test",
    srcs = ["cpu_profile_test.cc"],
    copts = ["-fno-access-control"],
    data = ["cpu_profile_sample"],
    deps = [
        ":cpu_profile",
        "@com_google_googletest//:gtest_main",
    ],
)

