// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "iree/testing/gtest.h"
#include "tools/iree-prof-output-utils.h"

namespace iree_prof {

TEST(IreeProfOutputTest, Empty) {
  std::vector<int64_t> merged;
  MergeDuration(1, 3, merged);
  EXPECT_EQ(merged, std::vector<int64_t>({1, 3}));
}

TEST(IreeProfOutputTest, NotOverlapped) {
  std::vector<int64_t> merged({1, 3, 11, 13});
  MergeDuration(5, 7, merged);
  EXPECT_EQ(merged, std::vector<int64_t>({1, 3, 5, 7, 11, 13}));
}

TEST(IreeProfOutputTest, OverlappedPartiallyWithStart) {
  std::vector<int64_t> merged({1, 3, 11, 13});
  MergeDuration(2, 7, merged);
  EXPECT_EQ(merged, std::vector<int64_t>({1, 7, 11, 13}));
}

TEST(IreeProfOutputTest, OverlappedPartiallyWithEnd) {
  std::vector<int64_t> merged({1, 3, 11, 13});
  MergeDuration(7, 12, merged);
  EXPECT_EQ(merged, std::vector<int64_t>({1, 3, 7, 13}));
}

TEST(IreeProfOutputTest, OverlappedPartiallyMultiple) {
  std::vector<int64_t> merged({1, 3, 11, 13});
  MergeDuration(2, 12, merged);
  EXPECT_EQ(merged, std::vector<int64_t>({1, 13}));
}

TEST(IreeProfOutputTest, Included) {
  std::vector<int64_t> merged({1, 7, 11, 13});
  MergeDuration(3, 5, merged);
  EXPECT_EQ(merged, std::vector<int64_t>({1, 7, 11, 13}));
}

TEST(IreeProfOutputTest, Including) {
  std::vector<int64_t> merged({1, 7, 11, 13});
  MergeDuration(-1, 9, merged);
  EXPECT_EQ(merged, std::vector<int64_t>({-1, 9, 11, 13}));
}

TEST(IreeProfOutputTest, IncludingEntirely) {
  std::vector<int64_t> merged({1, 7, 11, 13});
  MergeDuration(-1, 19, merged);
  EXPECT_EQ(merged, std::vector<int64_t>({-1, 19}));
}

}  // namespace iree_prof

// Define it's own main instead of gunit_main because gunit_main links tracy
// and duplicates symbols.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
