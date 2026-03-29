# Copyright 2025 Google LLC

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "native_assembler",
    srcs = ["native_assembler_wrapper.cc"],
    hdrs = ["native_assembler_wrapper.h"],
    copts = ["-O3"],
    deps = [
        "//riscv:rva23u64_bin_encoder",
        "//riscv:rva23u64_decoder",
        "@com_github_serge1_elfio//:elfio",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-sim//mpact/sim/util/asm:asm",
        "@com_google_mpact-sim//mpact/sim/util/asm:simple_assembler",
        "@com_googlesource_code_re2//:re2",
    ],
)
