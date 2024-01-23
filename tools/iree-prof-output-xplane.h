// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_PROF_OUTPUT_XPLANE_H_
#define IREE_PROF_OUTPUT_XPLANE_H_

#include <string>

#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "tools/iree-prof-output.h"

namespace iree_prof {

// Output IREE profiling results in a tracy worker to a Xplane protobuf file
// which can be loaded in the Tensorflow dashboard.
class IreeProfOutputXplane : public IreeProfOutput {
 public:
  explicit IreeProfOutputXplane(absl::string_view output_file_path);
  ~IreeProfOutputXplane() override;

  IreeProfOutputXplane(const IreeProfOutputXplane&) = delete;
  IreeProfOutputXplane& operator=(const IreeProfOutputXplane&) = delete;

  // IreeProfOutput implementation:
  absl::Status Output(tracy::Worker& worker) override;

 private:
  std::string output_file_path_;
};

}  // namespace iree_prof

#endif  // IREE_PROF_OUTPUT_XPLANE_H_
