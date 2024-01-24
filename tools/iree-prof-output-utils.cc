// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "tools/iree-prof-output-utils.h"

#include "third_party/abseil-cpp/absl/base/log_severity.h"
#include "third_party/abseil-cpp/absl/flags/parse.h"
#include "third_party/abseil-cpp/absl/log/globals.h"
#include "third_party/abseil-cpp/absl/log/initialize.h"
#include "third_party/abseil-cpp/absl/time/clock.h"
#include "third_party/abseil-cpp/absl/time/time.h"
#include "third_party/tracy/server/TracyWorker.hpp"

namespace iree_prof {

const char* GetZoneName(const tracy::Worker& worker,
                        int16_t source_location_id) {
  const auto& srcloc = worker.GetSourceLocation(source_location_id);
  return worker.GetString(srcloc.name.active ? srcloc.name : srcloc.function);
}

const char* GetThreadName(const tracy::Worker& worker, uint16_t tid) {
  return worker.GetThreadName(worker.DecompressThread(tid));
}

void YieldCpu() {
  absl::SleepFor(absl::Milliseconds(100));
}

std::vector<char*> InitializeLogAndParseCommandLine(int argc, char* argv[]) {
  // Set default logging level to stderr to INFO. It can be overridden by
  // --stderrthreshold flag.
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  absl::InitializeLog();
  return absl::ParseCommandLine(argc, argv);
}

}  // namespace iree_prof
