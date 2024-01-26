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

#include "third_party/tracy/server/TracyWorker.hpp"

namespace iree_prof {

// Polymorphizm functions for CPU and GPU zones.
int64_t GetEventStart(const tracy::ZoneEvent& event);
int64_t GetEventStart(const tracy::GpuEvent& event);

int64_t GetEventEnd(const tracy::ZoneEvent& event);
int64_t GetEventEnd(const tracy::GpuEvent& event);

template <typename T>
int64_t GetEventDuration(const T& event) {
  return GetEventEnd(event) - GetEventStart(event);
}

int GetThreadId(const tracy::Worker::ZoneThreadData& t);
int GetThreadId(const tracy::Worker::GpuZoneThreadData& t);

template <typename T>
std::string GetThreadName(const tracy::Worker& worker, int thread_id);
template <>
std::string GetThreadName<tracy::Worker::SourceLocationZones>(
    const tracy::Worker& worker, int thread_id);
template <>
std::string GetThreadName<tracy::Worker::GpuSourceLocationZones>(
    const tracy::Worker& worker, int thread_id);

// Returns the total duration of the thread of |thread_id|. It is the sum of
// durations of top-level zones.
template <typename T>
int64_t GetThreadDuration(const tracy::Worker& worker, int thread_id);
template <>
int64_t GetThreadDuration<tracy::Worker::SourceLocationZones>(
    const tracy::Worker& worker, int thread_id);
template <>
int64_t GetThreadDuration<tracy::Worker::GpuSourceLocationZones>(
    const tracy::Worker& worker, int thread_id);

// Returns the zone name associated to a source location ID in a trace worker.
const char* GetZoneName(const tracy::Worker& worker,
                        int16_t source_location_id);

// Yields CPU of current thread for a short while, 100 milliseconds.
void YieldCpu();

// Initializes log and parses command line flags.
// Returns all the remaining positional command line arguments.
std::vector<char*> InitializeLogAndParseCommandLine(int argc, char* argv[]);

}  // namespace iree_prof

#endif  // IREE_PROF_OUTPUT_UTILS_H_
