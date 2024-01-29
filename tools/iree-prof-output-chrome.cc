// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "tools/iree-prof-output-chrome.h"

#include <cstdint>
#include <fstream>
#include <vector>

#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/abseil-cpp/absl/strings/str_join.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/tracy/server/TracyWorker.hpp"
#include "tools/iree-prof-output-utils.h"

namespace iree_prof {
namespace {

// Chrome tracing viewer (https://github.com/catapult-project/catapult) format
// is described in
// https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/edit?usp=sharing.

constexpr int kPidFake = 0;

constexpr absl::string_view kTypeMetadata = "M";
constexpr absl::string_view kTypeEventStart = "B";
constexpr absl::string_view kTypeEventEnd = "E";

std::string GetSourceFileLineOrUnknown(const tracy::Worker& worker,
                                       int16_t source_location_id) {
  auto file_line = GetSourceFileLine(worker, source_location_id);
  return file_line.empty() ? "unknown" : file_line;
}

// Forward decl.
template <typename T>
void OutputTimeline(const tracy::Worker& worker,
                    uint16_t thread_id,
                    const tracy::Vector<tracy::short_ptr<T>>& timeline,
                    std::fstream& fout);

// Outputs a zone event as a JSON array entry.
void OutputEvent(absl::string_view name,
                 std::vector<absl::string_view> categories,
                 absl::string_view event_type,
                 int64_t timestamp_ns,
                 int thread_id,
                 std::vector<absl::string_view> args,
                 std::fstream& fout) {
  fout << "{";
  if (!name.empty()) {
    fout << "\"name\": \"" << name << "\", ";
  }
  if (!categories.empty()) {
    fout << "\"cat\": \"" << absl::StrJoin(categories, "\", \"") << "\", ";
  }
  fout << "\"ph\": \"" << event_type << "\", ";
  fout << "\"ts\": " << static_cast<double>(timestamp_ns) / 1000 << ", ";
  fout << "\"pid\": " << kPidFake << ", ";
  fout << "\"tid\": " << thread_id;
  if (!args.empty()) {
    fout << ", \"args\": {" << absl::StrJoin(args, ", ") << "}";
  }
  fout << "}";
}

// Returns a JSON key-value pair used for an event argument.
std::string ToArgField(absl::string_view key, absl::string_view value) {
  return absl::StrCat("\"", key, "\": \"", value, "\"");
}

// Outputs the zone events from a timeline which might be interleaved by zone
// events of the child timelines.
template <typename T>
void RealOutputTimeline(const tracy::Worker& worker,
                        uint16_t thread_id,
                        const tracy::Vector<T>& timeline,
                        std::fstream& fout) {
  for (const auto& e : timeline) {
    const auto& zone_event = GetEvent(e);
    auto zone_id = zone_event.SrcLoc();

    fout << ",\n";
    OutputEvent(GetZoneName(worker, zone_id), {}, kTypeEventStart,
                GetEventStart(zone_event), thread_id,
                {ToArgField("source", GetSourceFileLine(worker, zone_id))},
                fout);

    auto* children = GetEventChildren(worker, zone_event);
    if (children) {
      OutputTimeline(worker, thread_id, *children, fout);
    }

    fout << ",\n";
    OutputEvent("", {}, kTypeEventEnd, GetEventEnd(zone_event), thread_id, {},
                fout);
  }
}

// Outputs the zone events from a timeline and its child timelines by the help
// of RealOutputTimeline(). It's to differentiate templates of tracy::Vector<T>
// and ones of tracy::Vector<tracy::short_ptr<T>>.
template <typename T>
void OutputTimeline(const tracy::Worker& worker,
                    uint16_t thread_id,
                    const tracy::Vector<tracy::short_ptr<T>>& timeline,
                    std::fstream& fout) {
  if (timeline.is_magic()) {
    RealOutputTimeline(worker, thread_id,
                       *reinterpret_cast<const tracy::Vector<T>*>(&timeline),
                       fout);
  } else {
    RealOutputTimeline(worker, thread_id, timeline, fout);
  }
}

// Outputs the zone events running on a (CPU or GPU) thread into a chrome
// tracing viewer JSON file.
// A thread is represented by a root timeline and its compressed thread ID.
template <typename T>
void OutputThread(const tracy::Worker& worker,
                  uint16_t thread_id,
                  const tracy::Vector<tracy::short_ptr<T>>& timeline,
                  std::fstream& fout) {
  if (timeline.empty()) {
    return;
  }

  fout << ",\n";
  OutputEvent("thread_name", {}, kTypeMetadata, 0, thread_id,
              {ToArgField("name", GetThreadName<T>(worker, thread_id))}, fout);

  OutputTimeline(worker, thread_id, timeline, fout);
}

// Outputs a tracy worker into a chrome tracing viewer JSON file.
void OutputJson(const tracy::Worker& worker, std::fstream& fout) {
  fout << "[\n";
  OutputEvent("process_name", {}, kTypeMetadata, 0, 0,
              {ToArgField("name", worker.GetCaptureName())}, fout);

  for (const auto* d : worker.GetThreadData()) {
    OutputThread(worker,
                 const_cast<tracy::Worker*>(&worker)->CompressThread(d->id),
                 d->timeline, fout);
  }

  for (const auto& g : worker.GetGpuData()) {
    for (const auto& d : g->threadData) {
      OutputThread(worker, d.first, d.second.timeline, fout);
    }
  }
  fout << "\n]\n";
}

}  // namespace

IreeProfOutputChrome::IreeProfOutputChrome(absl::string_view output_file_path)
    : output_file_path_(output_file_path) {}

IreeProfOutputChrome::~IreeProfOutputChrome() = default;

absl::Status IreeProfOutputChrome::Output(tracy::Worker& worker) {
  std::fstream fout(output_file_path_.c_str(), std::ios::out|std::ios::binary);
  OutputJson(worker, fout);
  return absl::OkStatus();
}

}  // namespace iree_prof
