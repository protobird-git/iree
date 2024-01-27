// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "tools/iree-prof-output-xplane.h"

#include <cstdint>
#include <fstream>

#include "build_tools/third_party/tsl/xplane.pb.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/tracy/server/TracyWorker.hpp"
#include "tools/iree-prof-output-utils.h"

namespace iree_prof {
namespace {

// Forward decl.
template <typename T>
void ThreadToXline(const tracy::Worker& worker,
                   const tracy::Vector<tracy::short_ptr<T>>& timeline,
                   tensorflow::profiler::XPlane& xplane,
                   tensorflow::profiler::XLine& xline);

// Adds the zone events from a given timeline and its child timelines as Xlines.
template <typename T>
void RealThreadToXline(const tracy::Worker& worker,
                       const tracy::Vector<T>& timeline,
                       tensorflow::profiler::XPlane& xplane,
                       tensorflow::profiler::XLine& xline) {
  for (const auto& e : timeline) {
    const auto& zone_event = GetEvent(e);
    auto zone_id = zone_event.SrcLoc();

    auto& event_metadata = (*xplane.mutable_event_metadata())[zone_id];
    if (event_metadata.id() != zone_id) {
      event_metadata.set_id(zone_id);
      event_metadata.set_name(GetZoneName(worker, zone_id));
      event_metadata.set_display_name(event_metadata.name());
    }

    auto* event = xline.add_events();
    event->set_metadata_id(zone_id);
    event->set_offset_ps(GetEventStart(zone_event) * 1000);
    event->set_duration_ps(GetEventDuration(zone_event) * 1000);

    auto* children = GetEventChildren(worker, zone_event);
    if (children) {
      ThreadToXline(worker, *children, xplane, xline);
    }
  }
}

// Adds the zone events from a given timeline and its child timelines by the
// help of RealThreadToXline(). It's to differentiate templates of
// tracy::Vector<T> and ones of tracy::Vector<tracy::short_ptr<T>>.
template <typename T>
void ThreadToXline(const tracy::Worker& worker,
                   const tracy::Vector<tracy::short_ptr<T>>& timeline,
                   tensorflow::profiler::XPlane& xplane,
                   tensorflow::profiler::XLine& xline) {
  if (timeline.is_magic()) {
    RealThreadToXline(worker,
                      *reinterpret_cast<const tracy::Vector<T>*>(&timeline),
                      xplane, xline);
  } else {
    RealThreadToXline(worker, timeline, xplane, xline);
  }
}

// Adds the zone events running on a (CPU or GPU) thread into a JSON file.
// A thread is represented by a root timeline and its compressed thread ID.
template <typename T>
void ThreadToXplane(const tracy::Worker& worker,
                    uint16_t thread_id,
                    const tracy::Vector<tracy::short_ptr<T>>& timeline,
                    tensorflow::profiler::XPlane& xplane) {
  if (timeline.empty()) {
    return;
  }

  auto* xline = xplane.add_lines();
  xline->set_id(thread_id);
  xline->set_display_id(thread_id);
  xline->set_name(GetThreadName<T>(worker, thread_id));
  xline->set_display_name(xline->name());
  // Need to set xline->set_timestamp_ns() and xline->set_duration_ps()?

  ThreadToXline(worker, timeline, xplane, *xline);
}

// Returns an XSpace with an XPlane from a tracy worker.
tensorflow::profiler::XSpace ToXplane(const tracy::Worker& worker) {
  tensorflow::profiler::XSpace xspace;
  auto* xplane = xspace.add_planes();
  xplane->set_id(0);
  xplane->set_name(worker.GetCaptureName());

  // XLine corresponds to each Thread.
  // XEvent corresponds to each Zone.
  for (const auto* d : worker.GetThreadData()) {
    ThreadToXplane(worker,
                   const_cast<tracy::Worker*>(&worker)->CompressThread(d->id),
                   d->timeline, *xplane);
  }

  for (const auto& g : worker.GetGpuData()) {
    for (const auto& d : g->threadData) {
      ThreadToXplane(worker, d.first, d.second.timeline, *xplane);
    }
  }

  return xspace;
}

}  // namespace

IreeProfOutputXplane::IreeProfOutputXplane(absl::string_view output_file_path)
    : output_file_path_(output_file_path) {}

IreeProfOutputXplane::~IreeProfOutputXplane() = default;

absl::Status IreeProfOutputXplane::Output(tracy::Worker& worker) {
  std::fstream f(output_file_path_.c_str(), std::ios::out|std::ios::binary);
  if (!ToXplane(worker).SerializeToOstream(&f)) {
    return absl::UnavailableError(
        absl::StrCat("Could not write xplane file ", output_file_path_));
  }
  return absl::OkStatus();
}

}  // namespace iree_prof
