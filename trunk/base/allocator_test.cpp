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
