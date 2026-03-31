# Copyright 2025 Google LLC

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "native_assembler_test",
    size = "large",
    srcs = ["native_assembler_test.cc"],
    copts = ["-O3"],
    deps = [
        "//utils/assembler:native_assembler",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
