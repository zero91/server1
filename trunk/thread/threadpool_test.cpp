// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "thread/threadpool.hpp"
#include <sstream>

class ThreadPoolTest : public testing::Test {
 public:
  void Inc(int *cnt) {
    VLOG(2) << "Inc call, cnt: " << *cnt;
    *cnt = 0xbeef;
  }
 protected:
  static const int kPoolSize = 100;
};

TEST_F(ThreadPoolTest, Test1) {
  ThreadPool p("Test", kPoolSize);
  for (int k = 0; k < 1000; ++k) {
    p.Start();
    int item_size = kPoolSize * (rand() % 10 + 1);
    VLOG(2) << "item size: " << item_size;
    vector<int> v;
    v.resize(item_size, 0);
    for (int i = 0; i < item_size; ++i) {
      boost::function0<void> handler = boost::bind(
          &ThreadPoolTest::Inc, this, &v[i]);
      p.PushTask(handler);
    }
    p.Stop();
    for (int i = 0; i < item_size; ++i) {
      EXPECT_EQ(v[i], 0xbeef);
    }
    VLOG(2) << "thread stopped";
  }
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
