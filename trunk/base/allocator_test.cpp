// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "base/allocator.hpp"
#include <sstream>
class AllocatorTest : public testing::Test {
};
TEST_F(AllocatorTest, Test1) {
  static const int N = 1000;
  Allocator allocator;
  vector<void*> p;
  for (int i = 0; i < N; ++i) {
    p.push_back(allocator.Allocate(rand() % 1000 + 1));
    VLOG(2) << p.back();
  }
  for (int i = 0; i < N; ++i) {
    allocator.Deallocate(p[i]);
  }
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
