// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "tools/iree-prof-output-utils.h"

#include "third_party/tracy/server/TracyWorker.hpp"
#include "third_party/abseil-cpp/absl/time/clock.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace iree_prof {

const char* GetZoneName(const tracy::Worker& worker,
                        int16_t source_location_id) {
  const auto& srcloc = worker.GetSourceLocation(source_location_id);
  return worker.GetString(srcloc.name.active ? srcloc.name : srcloc.function);
}

void YieldCpu() {
  absl::SleepFor(absl::Milliseconds(100));
}

}  // namespace iree_prof
