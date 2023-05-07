// Copyright 2023 The OpenXLA Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "mlir/Pass/PassManager.h"

namespace openxla::partitioner {

// Builds a pipeline which runs the GSPMD partitioner.
void buildGSPMDPipeline(mlir::PassManager &passManager);

}  // namespace openxla::partitioner
