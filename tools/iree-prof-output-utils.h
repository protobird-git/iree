// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_PROF_OUTPUT_UTILS_H_
#define IREE_PROF_OUTPUT_UTILS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/tracy/server/TracyWorker.hpp"

namespace iree_prof {

// Util templates to mitigate the difference between
// tracy::Worker::SourceLocationZone and tracy::Worker::GpuSourceLocationZone.
template <typename E>
using TimestampFunc = int64_t (E::*)() const;

template <typename T>
struct TracyZoneFunctions {
  TimestampFunc<tracy::ZoneEvent> start = &tracy::ZoneEvent::Start;
  TimestampFunc<tracy::ZoneEvent> end = &tracy::ZoneEvent::End;

  int GetThreadId(const tracy::Worker::ZoneThreadData& t) { return t.Thread(); }

  std::string GetThreadName(const tracy::Worker& worker, int tid) {
    return worker.GetThreadName(worker.DecompressThread(tid));
  }
};

template <>
struct TracyZoneFunctions<tracy::Worker::GpuSourceLocationZones> {
  TimestampFunc<tracy::GpuEvent> start = &tracy::GpuEvent::GpuStart;
  TimestampFunc<tracy::GpuEvent> end = &tracy::GpuEvent::GpuEnd;

  // Negates the actual thread ID to distinguish it from one in CPU zones.
  int GetThreadId(const tracy::Worker::GpuZoneThreadData& t) {
    return -static_cast<int>(t.Thread());
  }

  std::string GetThreadName(const tracy::Worker& worker, int tid) {
    return absl::StrCat("gpu-thread-", -tid);
  }
};

// Returns the zone name associated to a source location ID in a trace worker.
const char* GetZoneName(const tracy::Worker& worker,
                        int16_t source_location_id);

// Merges a duration, i.e. |end| - |start| into merged_duration vector which
// consists of starts and ends. For example,
// 1) start = 1, end = 3, merged = empty        -> result = {1, 3}
// 2) start = 6, end = 8, merged = {1, 3}       -> result = {1, 3, 6, 8}
// 3) start = 2, end = 7, merged = {1, 3, 6, 8} -> result = {1, 8}
// 4) start = 7, end = 10, merged = {1, 8}      -> result = {1, 10}
// 5) start = 10, end = 11, merged = {1, 10}    -> result = {1, 11}
// A merged duration is a sorted vector of pairs of starts and ends.
void MergeDuration(int64_t start, int64_t end, std::vector<int64_t>& merged);

// Gets the total duration from merged durations, i.e a vector of pairs of
// starts and ends.
int64_t SumMergedDuration(const std::vector<int64_t>& merged);

// Yields CPU of current thread for a short while, 100 milliseconds.
void YieldCpu();

// Initializes log and parses command line flags.
// Returns all the remaining positional command line arguments.
std::vector<char*> InitializeLogAndParseCommandLine(int argc, char* argv[]);

}  // namespace iree_prof

#endif  // IREE_PROF_OUTPUT_UTILS_H_
