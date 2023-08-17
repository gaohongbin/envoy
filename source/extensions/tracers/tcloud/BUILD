load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

# Trace driver for TCloud

envoy_extension_package()

envoy_cc_extension(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    category = "envoy.tracers",
    security_posture = "robust_to_untrusted_downstream",
    deps = [
        ":tcloud_tracer_impl",
        "//source/extensions/tracers/common:factory_base_lib",
        "@envoy_api//envoy/config/trace/v3:pkg_cc_proto",
    ],
)

envoy_cc_library(
    name = "tcloud_tracer_impl",
    srcs = ["tcloud_tracer_impl.cc"],
    hdrs = ["tcloud_tracer_impl.h"],
    copts = ["-Wno-unused-parameter"],
    deps = [
        "//source/common/config:utility_lib",
        "//source/common/tracing:http_tracer_lib",
        "@envoy_api//envoy/config/trace/v3:pkg_cc_proto",
    ],
)
