# Copyright 2023 The IREE Authors
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

find_package(Protobuf REQUIRED)
protobuf_generate_cpp(
  XPLANE_PROTO_SRCS
  XPLANE_PROTO_HDRS
  ${IREE_ROOT_DIR}/third_party/tsl/tsl/profiler/protobuf/xplane.proto
)

# The set of rules below is equivalent to:
#
# external_cc_library(
#   PACKAGE
#     tsl
#   NAME
#     xplane
#   ROOT
#     ""
#   SRCS
#     ${XPLANE_PROTO_SRCS}
#   HRDS
#     ${XPLANE_PROTO_HDRS}
# )
#
# which doesn't work for protobuf because of target_sources() which requires
# files already exist, but not be generated on the fly.
#
add_library(tsl_xplane STATIC ${XPLANE_PROTO_SRCS} ${XPLANE_PROTO_HDRS})
target_compile_options(tsl_xplane PRIVATE ${IREE_DEFAULT_COPTS})
target_link_libraries(tsl_xplane PUBLIC ${Protobuf_LIBRARIES})
set_property(TARGET tsl_xplane PROPERTY FOLDER third_party/internal)
set_property(TARGET tsl_xplane PROPERTY CXX_STANDARD ${IREE_CXX_STANDARD})
set_property(TARGET tsl_xplane PROPERTY CXX_STANDARD_REQUIRED ON)
add_library(tsl::xplane ALIAS tsl_xplane)
