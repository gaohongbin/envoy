load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "tcloud_map_lib",
    srcs = ["tcloud_map_impl.cc"],
    hdrs = ["tcloud_map_impl.h"],
    deps = [
        "//include/envoy/tcloud:tcloud_map_interface",
    ],
)
