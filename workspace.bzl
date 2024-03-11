load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")


def _maybe(repo_rule, name, **kwargs):
    if not native.existing_rule(name):
        repo_rule(name = name, **kwargs)

def pprofcpp_workspace():
    native.new_local_repository(
        name = "macos_includes",
        path = "/opt/homebrew/opt/binutils/include",
        build_file_content = """
cc_library(
name = "includes",
hdrs = glob(["**/*.h"]),
includes = ["."],
visibility = ["//visibility:public"],
)
        """,
    )
    http_archive(
            name = "fmtlib",
            sha256 = "5dea48d1fcddc3ec571ce2058e13910a0d4a6bab4cc09a809d8b1dd1c88ae6f2",
            strip_prefix = "fmt-9.1.0",
            build_file = "//third_party/fmtlib:fmtlib.BUILD",
            urls = ["https://github.com/fmtlib/fmt/archive/9.1.0.tar.gz",],
    )
    http_archive(
        name = "com_github_gflags_gflags",
        sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
        strip_prefix = "gflags-2.2.2",
        urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
    )
    http_archive(
        name = "com_google_googletest",
        sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
        strip_prefix = "googletest-release-1.10.0",
        build_file = "//third_party/gtest:BUILD",
        urls = ["https://github.com/google/googletest/archive/release-1.10.0.tar.gz"],
    )
 

