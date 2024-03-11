package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "all_files",
    srcs = glob(
        ["**/*"]
    ),
)

cc_binary(
    name = "profile_printer",
    srcs = ["profile_printer.cc"],
    deps = [
        "//profiling:cpu_profile",
    ],
)

cc_binary(
    name = "addr2symbol",
    srcs = ["addr2symbol.cc"],
    deps = [
        "//profiling/symbol:profile_symbol",
        "@com_github_gflags_gflags//:gflags",
    ],
)
