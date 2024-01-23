// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "tools/iree-prof-output.h"

#include <string>

#include "third_party/abseil-cpp/absl/flags/flag.h"
#include "third_party/abseil-cpp/absl/log/log.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/tracy/server/TracyWorker.hpp"
#include "tools/iree-prof-output-stdout.h"
#include "tools/iree-prof-output-tracy.h"
#include "tools/iree-prof-output-xplane.h"

ABSL_FLAG(std::string, output_tracy_file, "",
          "Tracy file to write as the output of the given executable command.");
ABSL_FLAG(std::string, output_xplane_file, "",
          "Xplane file to write as the output of execution or conversion.");
ABSL_FLAG(bool, output_stdout, true,
          "Whether to print Tracy result to stdout.");

namespace iree_prof {
namespace {

void LogStatusIfError(const absl::Status& status) {
  if (!status.ok()) {
    LOG(ERROR) << status;
  }
}

}  // namespace

void Output(tracy::Worker& worker) {
  if (absl::GetFlag(FLAGS_output_stdout)) {
    LogStatusIfError(IreeProfOutputStdout().Output(worker));
  }

  std::string output_tracy_file = absl::GetFlag(FLAGS_output_tracy_file);
  if (!output_tracy_file.empty()) {
    LogStatusIfError(IreeProfOutputTracy(output_tracy_file).Output(worker));
  }

  std::string output_xplane_file = absl::GetFlag(FLAGS_output_xplane_file);
  if (!output_xplane_file.empty()) {
    LogStatusIfError(IreeProfOutputXplane(output_xplane_file).Output(worker));
  }
}

}  // namespace iree_prof
