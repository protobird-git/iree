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

bool HasSubstr(absl::string_view str, const std::vector<std::string>& substrs) {
  return std::find_if(
             substrs.begin(), substrs.end(),
             [str](const std::string& s) { return str.find(s) != str.npos; })
             != substrs.end();
}

template <typename T>
struct Zone {
  const char* name;
  const T* zone;
};

template <typename T>
std::vector<Zone<T>> GetZonesFilteredAndSorted(
    const tracy::Worker& worker,
    const tracy::unordered_flat_map<int16_t, T>& zones,
    const std::vector<std::string>& zone_substrs) {
  std::map<int, Zone<T>> ordered_map;
  for (const auto& z : zones) {
    const char* zone_name = GetZoneName(worker, z.first);
    if (HasSubstr(zone_name, zone_substrs)) {
      ordered_map[z.second.total] = Zone<T>{zone_name, &z.second};
    }
  }

  std::vector<Zone<T>> vector_to_return;
  for (auto it = ordered_map.rbegin(); it != ordered_map.rend(); ++it) {
    vector_to_return.push_back(it->second);
  }
  return vector_to_return;
};

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

template <typename T>
void OutputToStdout(const tracy::Worker& worker,
                    const Zone<T>& zone,
                    absl::string_view header,
                    IreeProfOutputStdout::DurationUnit unit) {
  absl::flat_hash_map<uint16_t, int64_t> ns_per_thread;
  TracyZoneFunctions<T> func;
  for (const auto& t : zone.zone->zones) {
    ns_per_thread[t.Thread()] +=
        (t.Zone()->*func.end)() - (t.Zone()->*func.start)();
  }

  std::cout << header << zone.name
            << ": count=" << zone.zone->zones.size()
            << ", total=" << GetDurationStr(zone.zone->total, unit);
  for (auto it : ns_per_thread) {
    std::cout << ", " << worker.GetThreadName(worker.DecompressThread(it.first))
              << "=" << GetDurationStr(it.second, unit);
  }
  std::cout << "\n";
}

}  // namespace

IreeProfOutputStdout::IreeProfOutputStdout(
    const std::vector<std::string>& zone_substrs,
    DurationUnit unit)
    : zone_substrs_(zone_substrs), unit_(unit) {}

IreeProfOutputStdout::~IreeProfOutputStdout() = default;

absl::Status IreeProfOutputStdout::Output(tracy::Worker& worker) {
  std::cout << "[TRACY    ] CaptureName = " << worker.GetCaptureName() << "\n";
  std::cout << "[TRACY    ]     CpuArch = " << ArchToString(worker.GetCpuArch())
            << "\n";

  std::cout << "[TRACY-CPU]   CPU Zones = " << worker.GetZoneCount() << "\n";
  auto cpu_zones = GetZonesFilteredAndSorted(
      worker, worker.GetSourceLocationZones(), zone_substrs_);
  for (const auto& z : cpu_zones) {
    OutputToStdout(worker, z, "[TRACY-CPU]         ", unit_);
  }

  std::cout << "[TRACY-GPU]   GPU Zones = " << worker.GetGpuZoneCount() << "\n";
  auto gpu_zones = GetZonesFilteredAndSorted(
      worker, worker.GetGpuSourceLocationZones(), zone_substrs_);
  for (const auto& z : gpu_zones) {
    OutputToStdout(worker, z, "[TRACY-GPU]         ", unit_);
  }

  return absl::OkStatus();
}

}  // namespace iree_prof
