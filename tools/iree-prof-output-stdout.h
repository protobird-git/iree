// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_PROF_OUTPUT_STDOUT_H_
#define IREE_PROF_OUTPUT_STDOUT_H_

#include "tools/iree-prof-output.h"

namespace iree_prof {

// Output IREE profiling results in a tracy worker to stdout.
class IreeProfOutputStdout : public IreeProfOutput {
 public:
  IreeProfOutputStdout();
  ~IreeProfOutputStdout() override;

  IreeProfOutputStdout(const IreeProfOutputStdout&) = delete;
  IreeProfOutputStdout& operator=(const IreeProfOutputStdout&) = delete;

  // IreeProfOutput implementation:
  absl::Status Output(tracy::Worker& worker) override;
};

}  // namespace iree_prof

#endif  // IREE_PROF_OUTPUT_STDOUT_H_
