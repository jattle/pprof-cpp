package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "all_files",
    srcs = glob(
        ["**/*"]
    ),
)

config_setting(
    name = "is_linux",
    values = {"cpu": "k8"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "is_macos",
    #values = {"cpu": "Darwin"},
    constraint_values = ["@platforms//os:macos"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "profile_symbol",
    hdrs = ["profile_symbol.h"],
    srcs = ["profile_symbol.cc"],
    deps = [
        "@fmtlib//:fmtlib",
        "//profiling/util:utils",
            ] +
    select({
        ":is_linux": [
        ],
        ":is_macos": [
            "@macos_includes//:includes",
        ],
    }),
    linkopts = ["-lbfd","-liberty"],
)

cc_test(
    name = "profile_symbol_test",
    srcs = ["profile_symbol_test.cc"],
    copts = ["-fno-access-control"],
    data = ["//profiling/io:cpu_profile_sample"],
    deps = [
        ":profile_symbol",
        "@com_google_googletest//:gtest_main",
    ],
)
