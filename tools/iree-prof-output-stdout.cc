// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "tools/iree-prof-output-stdout.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <vector>

#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/tracy/public/common/TracyProtocol.hpp"
#include "third_party/tracy/server/TracyWorker.hpp"
#include "tools/iree-prof-output-utils.h"

namespace iree_prof {
namespace {

template <typename T>
struct Zone {
  int16_t source_location_id;
  const T* zone;
};

template <typename T>
std::vector<Zone<T>> GetTopZones(
    const tracy::unordered_flat_map<int16_t, T>& zones,
    int num_zones_to_return) {
  std::map<int, Zone<T>> ordered_map;
  for (const auto& z : zones) {
    ordered_map[z.second.total] = Zone<T>{z.first, &z.second};
    if (ordered_map.size() > num_zones_to_return) {
      ordered_map.erase(ordered_map.begin());
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

}  // namespace

IreeProfOutputStdout::IreeProfOutputStdout() = default;
IreeProfOutputStdout::~IreeProfOutputStdout() = default;

absl::Status IreeProfOutputStdout::Output(tracy::Worker& worker) {
  std::cout << "[TRACY    ] CaptureName = " << worker.GetCaptureName() << "\n";
  std::cout << "[TRACY    ]     CpuArch = " << ArchToString(worker.GetCpuArch())
            << "\n";

  std::cout << "[TRACY-CPU]   CPU Zones = " << worker.GetZoneCount() << "\n";
  auto cpu_zones = GetTopZones(worker.GetSourceLocationZones(), 10);
  for (const auto& z : cpu_zones) {
    std::cout << "[TRACY-CPU]         "
              << GetZoneName(worker, z.source_location_id)
              << ": total_ns=" << z.zone->total
              << ", count=" << z.zone->zones.size() << "\n";
  }

  std::cout << "[TRACY-GPU]   GPU Zones = " << worker.GetGpuZoneCount() << "\n";
  auto gpu_zones = GetTopZones(worker.GetGpuSourceLocationZones(), 10);
  for (const auto& z : gpu_zones) {
    std::cout << "[TRACY-GPU]         "
              << GetZoneName(worker, z.source_location_id)
              << ": total_ns=" << z.zone->total
              << ", count=" << z.zone->zones.size() << "\n";
  }

  return absl::OkStatus();
}

}  // namespace iree_prof
