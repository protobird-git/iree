// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_PROF_OUTPUT_UTILS_H_
#define IREE_PROF_OUTPUT_UTILS_H_

#include <cstdint>

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
};

template <>
struct TracyZoneFunctions<tracy::Worker::GpuSourceLocationZones> {
  TimestampFunc<tracy::GpuEvent> start = &tracy::GpuEvent::GpuStart;
  TimestampFunc<tracy::GpuEvent> end = &tracy::GpuEvent::GpuEnd;
};

// Returns the zone name associated to a source location ID in a trace worker.
const char* GetZoneName(const tracy::Worker& worker,
                        int16_t source_location_id);

// Returns the thread name for the given tid.
const char* GetThreadName(const tracy::Worker& worker, uint16_t tid);

// Yields CPU of current thread for a short while, 100 milliseconds.
void YieldCpu();

}  // namespace iree_prof

#endif  // IREE_PROF_OUTPUT_UTILS_H_
