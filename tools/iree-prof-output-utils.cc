// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "tools/iree-prof-output-utils.h"

#include "third_party/abseil-cpp/absl/base/log_severity.h"
#include "third_party/abseil-cpp/absl/flags/parse.h"
#include "third_party/abseil-cpp/absl/log/check.h"
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

void MergeDuration(int64_t start, int64_t end, std::vector<int64_t>& merged) {
  CHECK_LE(start, end);
  if (start == end) {
    return;  // No changes.
  }

  int least_index_greater_than_or_equal_to_start = 0;
  while (least_index_greater_than_or_equal_to_start < merged.size() &&
         start > merged[least_index_greater_than_or_equal_to_start]) {
    ++least_index_greater_than_or_equal_to_start;
  }

  int greatest_index_less_than_or_equal_to_end = merged.size() - 1;
  while (greatest_index_less_than_or_equal_to_end >= 0 &&
         end < merged[greatest_index_less_than_or_equal_to_end]) {
    --greatest_index_less_than_or_equal_to_end;
  }

  // least_index_greater_than_or_equal_to_start can be at most
  // greatest_index_less_than_or_equal_to_end + 1.
  CHECK_LE(least_index_greater_than_or_equal_to_start,
           greatest_index_less_than_or_equal_to_end + 1);

  bool start_is_within_an_existing_duration =
      least_index_greater_than_or_equal_to_start % 2 != 0;
  bool end_is_within_an_existing_duration =
      greatest_index_less_than_or_equal_to_end % 2 == 0;

  // Merge all durations between start, end into one duration
  merged.erase(merged.begin() + least_index_greater_than_or_equal_to_start,
               merged.begin() + greatest_index_less_than_or_equal_to_end + 1);

  if (!end_is_within_an_existing_duration) {
    merged.insert(merged.begin() + least_index_greater_than_or_equal_to_start,
                  end);
  }

  if (!start_is_within_an_existing_duration) {
    merged.insert(merged.begin() + least_index_greater_than_or_equal_to_start,
                  start);
  }
}

int64_t SumMergedDuration(const std::vector<int64_t>& merged) {
  int64_t total = 0;
  for (int i = 0; i < merged.size(); i += 2) {
    CHECK_LT(i+1, merged.size());
    CHECK_LE(merged[i], merged[i+1]);
    if (i > 0) {
      CHECK_LE(merged[i-1], merged[i]);
    }
    total += merged[i+1] - merged[i];
  }
  return total;
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
