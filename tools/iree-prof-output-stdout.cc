// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "tools/iree-prof-output-stdout.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>

#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/tracy/public/common/TracyProtocol.hpp"
#include "third_party/tracy/server/TracyWorker.hpp"
#include "tools/iree-prof-output-utils.h"

namespace iree_prof {
namespace {

const char* ArchToString(tracy::CpuArchitecture arch) {
  switch (arch) {
    case tracy::CpuArchUnknown: return "Unknown";
    case tracy::CpuArchX86: return "x86";
    case tracy::CpuArchX64: return "x86_64";
    case tracy::CpuArchArm32: return "arm";
    case tracy::CpuArchArm64: return "aarch64";
    default: return "Unknown";
  }
}

bool HasSubstr(absl::string_view str, const std::vector<std::string>& substrs) {
  return std::find_if(
             substrs.begin(), substrs.end(),
             [str](const std::string& s) { return str.find(s) != str.npos; })
             != substrs.end();
}

std::string GetDurationStr(int64_t duration_ns,
                           IreeProfOutputStdout::DurationUnit unit) {
  switch (unit) {
    case IreeProfOutputStdout::DurationUnit::kNanoseconds:
      return absl::StrCat(duration_ns, "ns");
    case IreeProfOutputStdout::DurationUnit::kMicroseconds:
      return absl::StrCat(static_cast<double>(duration_ns) / 1000, "us");
    case IreeProfOutputStdout::DurationUnit::kSeconds:
      return absl::StrCat(static_cast<double>(duration_ns) / 1000000000, "s");
    case IreeProfOutputStdout::DurationUnit::kMilliseconds:
    default:
      return absl::StrCat(static_cast<double>(duration_ns) / 1000000, "ms");
  }
}

struct LongestDuration {
  const char* zone_name;
  int64_t duration;
};

template <typename T>
absl::flat_hash_map<int, LongestDuration> GetLongestDurationPerThread(
    const tracy::Worker& worker,
    const tracy::unordered_flat_map<int16_t, T>& zones,
    const std::vector<std::string>& thread_substrs) {
  absl::flat_hash_map<int, LongestDuration> longest_durations;
  TracyZoneFunctions<T> func;
  for (const auto& z : zones) {
    for (const auto& t : z.second.zones) {
      auto tid = t.Thread();
      auto duration = (t.Zone()->*func.end)() - (t.Zone()->*func.start)();
      if (!longest_durations.contains(tid) ||
          duration > longest_durations[tid].duration) {
        longest_durations[tid] = {GetZoneName(worker, z.first), duration};
      }
    }
  }

  // Filters threads matched with subscrings in thread_substrs.
  if (!thread_substrs.empty()) {
    absl::flat_hash_map<int, LongestDuration> filtered_durations;
    for (auto& it : longest_durations) {
      if (HasSubstr(GetThreadName(worker, it.first), thread_substrs)) {
        filtered_durations[it.first] = it.second;
      }
    }
    longest_durations.swap(filtered_durations);
  }

  return longest_durations;
}

template <typename T>
struct Zone {
  const char* name;
  const T* zone;
  int64_t count;
  int64_t total_duration;
};

template <typename T>
std::vector<Zone<T>> GetZonesFilteredAndSorted(
    const tracy::Worker& worker,
    const tracy::unordered_flat_map<int16_t, T>& zones,
    const std::vector<std::string>& zone_substrs,
    const absl::flat_hash_map<int, LongestDuration>& duration_per_thread) {
  std::vector<Zone<T>> zones_filtered;
  TracyZoneFunctions<T> func;
  for (const auto& z : zones) {
    const char* zone_name = GetZoneName(worker, z.first);
    if (!HasSubstr(zone_name, zone_substrs)) {
      continue;
    }

    int64_t count = 0;
    int64_t total_duration = 0;
    for (const auto& t : z.second.zones) {
      if (duration_per_thread.contains(t.Thread())) {
        ++count;
        total_duration += (t.Zone()->*func.end)() - (t.Zone()->*func.start)();
      }
    }

    if (count == 0 || total_duration == 0) {
      continue;
    }

    zones_filtered.emplace_back(
        Zone<T>{zone_name, &z.second, count, total_duration});
  }

  std::sort(zones_filtered.begin(), zones_filtered.end(),
            [](const Zone<T>& a, const Zone<T>& b) {
              // Sort in a descending order.
              return a.total_duration > b.total_duration;
            });

  return zones_filtered;
};

int GetColOfThread(const std::vector<std::string>& headers,
                   const char* thread_name) {
  for (int i = 3; i < headers.size(); ++i) {
    if (headers[i] == thread_name) {
      return i;
    }
  }
  return headers.size();  // Return a wrong index intentially.
}

std::string GetPercentage(int64_t num, int64_t total) {
  double percentage = static_cast<double>(num * 10000 / total) / 100;
  return absl::StrCat("(", percentage, "%)");
}

// Returns the total duration of zone used for sorting later.
template <typename T>
void FillOutputTableRowWithZone(
    const tracy::Worker& worker,
    const Zone<T>& zone,
    int64_t total_duration,
    const absl::flat_hash_map<int, LongestDuration>& duration_per_thread,
    IreeProfOutputStdout::DurationUnit unit,
    const std::vector<std::string>& headers,
    std::vector<std::string>& output_row) {
  absl::flat_hash_map<uint16_t, int64_t> ns_per_thread;
  TracyZoneFunctions<T> func;
  for (const auto& t : zone.zone->zones) {
    auto tid = t.Thread();
    if (duration_per_thread.contains(tid)) {
      ns_per_thread[tid] += (t.Zone()->*func.end)() - (t.Zone()->*func.start)();
    }
  }

  output_row[0] = zone.name;
  output_row[1] = absl::StrCat(zone.count);
  output_row[2] = absl::StrCat(
      GetDurationStr(zone.total_duration, unit),
      GetPercentage(zone.total_duration, total_duration));
  for (auto it : ns_per_thread) {
    output_row[GetColOfThread(headers, GetThreadName(worker, it.first))] =
        absl::StrCat(GetDurationStr(it.second, unit),
                     GetPercentage(it.second,
                                   duration_per_thread.at(it.first).duration));
  }
}

template <typename T>
std::vector<std::vector<std::string>> BuildOutputTable(
    const tracy::Worker& worker,
    const std::vector<Zone<T>>& zones,
    int64_t total_duration,
    const absl::flat_hash_map<int, LongestDuration>& duration_per_thread,
    IreeProfOutputStdout::DurationUnit unit) {
  // 1st row is for headers, 2nd row is for durations of zones per thread.
  auto num_rows = zones.size() + 2;
  // 1st col is for zone names, 2nd is for counts, 3rd is for total durations.
  auto num_cols = duration_per_thread.size() + 3;

  std::vector<std::vector<std::string>> output_table(num_rows);

  auto& headers = output_table[0];
  headers.reserve(num_cols);
  headers.push_back("Zone");
  headers.push_back("Count");
  headers.push_back("Total");
  for (const auto& it : duration_per_thread) {
    headers.push_back(GetThreadName(worker, it.first));
  }
  std::sort(headers.begin() + 3, headers.end());

  auto& totals = output_table[1];
  totals.resize(num_cols);
  totals[0] = "Duration";
  // totals[1] is empty since count is not a duration.
  totals[2] = GetDurationStr(total_duration, unit);
  for (const auto& it : duration_per_thread) {
    totals[GetColOfThread(headers, GetThreadName(worker, it.first))] =
        GetDurationStr(it.second.duration, unit);
  }

  auto output_row = output_table.begin() + 2;
  for (const auto& z : zones) {
    output_row->resize(num_cols);
    FillOutputTableRowWithZone(worker, z, total_duration, duration_per_thread,
                               unit, headers, *(output_row++));
  }

  return output_table;
}

template <typename T>
void OutputToStdout(
    const tracy::Worker& worker,
    const tracy::unordered_flat_map<int16_t, T>& zones,
    const std::vector<std::string>& zone_substrs,
    const std::vector<std::string>& thread_substrs,
    absl::string_view header,
    IreeProfOutputStdout::DurationUnit unit) {
  if (zones.empty()) {
    return;
  }

  auto duration_per_thread =
      GetLongestDurationPerThread(worker, zones, thread_substrs);
  if (duration_per_thread.empty()) {
    return;
  }

  int64_t total = 0;
  for (const auto& it : duration_per_thread) {
    total += it.second.duration;
  }

  auto zones_filtered = GetZonesFilteredAndSorted(worker, zones, zone_substrs,
                                                  duration_per_thread);
  auto output_table = BuildOutputTable(worker, zones_filtered,
                                       total, duration_per_thread, unit);

  std::vector<int> widths(output_table[0].size());
  for (const auto& row : output_table) {
    for (int i = 0; i < row.size(); ++ i) {
      if (row[i].size() > widths[i]) {
        widths[i] = row[i].size();
      }
    }
  }

  for (const auto& row : output_table) {
    std::cout << header << "      ";
    for (int i = 0; i < row.size(); ++ i) {
      std::cout << row[i] << std::string(widths[i] - row[i].size() + 1, ' ');
    }
    std::cout << "\n";
  }
}

}  // namespace

IreeProfOutputStdout::IreeProfOutputStdout(
    const std::vector<std::string>& zone_substrs,
    const std::vector<std::string>& thread_substrs,
    DurationUnit unit)
    : zone_substrs_(zone_substrs),
      thread_substrs_(thread_substrs),
      unit_(unit) {}

IreeProfOutputStdout::~IreeProfOutputStdout() = default;

absl::Status IreeProfOutputStdout::Output(tracy::Worker& worker) {
  std::cout << "[TRACY    ] CaptureName: " << worker.GetCaptureName() << "\n";
  std::cout << "[TRACY    ]     CpuArch: " << ArchToString(worker.GetCpuArch())
            << "\n";

  std::cout << "[TRACY-CPU]   CPU Zones: " << worker.GetZoneCount() << "\n";
  OutputToStdout(worker, worker.GetSourceLocationZones(), zone_substrs_,
                 thread_substrs_, "[TRACY-CPU]", unit_);

  std::cout << "[TRACY-GPU]   GPU Zones = " << worker.GetGpuZoneCount() << "\n";
  OutputToStdout(worker, worker.GetGpuSourceLocationZones(), zone_substrs_,
                 thread_substrs_, "[TRACY-GPU]", unit_);

  return absl::OkStatus();
}

}  // namespace iree_prof
