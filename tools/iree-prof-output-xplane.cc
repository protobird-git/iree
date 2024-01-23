// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "tools/iree-prof-output-xplane.h"

#include <cstdint>
#include <fstream>

#include "build_tools/third_party/tsl/xplane.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/tracy/server/TracyWorker.hpp"
#include "tools/iree-prof-output-utils.h"

namespace iree_prof {
namespace {

tensorflow::profiler::XSpace ToXplane(const tracy::Worker& worker) {
  tensorflow::profiler::XSpace xspace;
  auto* xplane = xspace.add_planes();
  xplane->set_id(0);
  xplane->set_name(worker.GetCaptureName());

  // XLine corresponds to each Thread.
  // XEvent corresponds to each Zone.
  absl::flat_hash_map<uint16_t, tensorflow::profiler::XLine*> xlines;

  for (const auto& z : worker.GetSourceLocationZones()) {
    auto& event_metadata = (*xplane->mutable_event_metadata())[z.first];
    event_metadata.set_id(z.first);
    event_metadata.set_name(GetZoneName(worker, z.first));
    event_metadata.set_display_name(event_metadata.name());

    for (const auto& t : z.second.zones) {
      auto tid = t.Thread();
      if (!xlines.contains(tid)) {
        auto* xline = xplane->add_lines();
        xline->set_id(tid);
        xline->set_display_id(tid);
        xline->set_name(worker.GetThreadName(worker.DecompressThread(tid)));
        xline->set_display_name(xline->name());
        // Need to set xline->set_timestamp_ns() and xline->set_duration_ps()?
        xlines[tid] = xline;
      }

      auto* event = xlines[tid]->add_events();
      event->set_metadata_id(z.first);
      event->set_offset_ps(t.Zone()->Start() * 1000);
      event->set_duration_ps((t.Zone()->End() - t.Zone()->Start()) * 1000);
    }
  }

  for (const auto& z : worker.GetGpuSourceLocationZones()) {
    auto& event_metadata = (*xplane->mutable_event_metadata())[z.first];
    event_metadata.set_id(z.first);
    event_metadata.set_name(GetZoneName(worker, z.first));
    event_metadata.set_display_name(event_metadata.name());

    for (const auto& t : z.second.zones) {
      auto tid = t.Thread();
      if (!xlines.contains(tid)) {
        auto* xline = xplane->add_lines();
        xline->set_id(tid);
        xline->set_display_id(tid);
        xline->set_name(worker.GetThreadName(worker.DecompressThread(tid)));
        xline->set_display_name(xline->name());
        // Need to set xline->set_timestamp_ns() and xline->set_duration_ps()?
        xlines[tid] = xline;
      }

      auto* event = xlines[tid]->add_events();
      event->set_metadata_id(z.first);
      event->set_offset_ps(t.Zone()->GpuStart() * 1000);
      event->set_duration_ps(
          (t.Zone()->GpuEnd() - t.Zone()->GpuStart()) * 1000);
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
