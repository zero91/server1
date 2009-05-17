// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "server/io_service_pool.hpp"
#include <sstream>

class IOServicePoolTest : public testing::Test {
 public:
  void Inc(int *cnt) {
    VLOG(2) << "Inc call, cnt: " << *cnt;
    *cnt = 0xbeef;
  }
  void Inc2(int *cnt) {
    VLOG(2) << "Inc2 call, cnt: " << *cnt;
    sleep(rand() % 100 + 1);
    *cnt = 0xbeef;
  }
 protected:
  static const int kPoolSize = 100;
};

TEST_F(IOServicePoolTest, Test1) {
  IOServicePool p("Test", kPoolSize);
  for (int k = 0; k < 1000; ++k) {
    p.Start();
    int item_size = kPoolSize * (rand() % 10 + 1);
    VLOG(2) << "item size: " << item_size;
    EXPECT_GT(item_size, 0);
    vector<int> v;
    v.resize(item_size, 0);
    for (int i = 0; i < item_size; ++i) {
      boost::function0<void> handler = boost::bind(
          &IOServicePoolTest::Inc, this, &v[i]);
      p.get_io_service().dispatch(handler);
    }
    p.Stop();
    for (int i = 0; i < item_size; ++i) {
      EXPECT_EQ(v[i], 0xbeef);
    }
    VLOG(2) << "thread stopped";
  }
}

TEST_F(IOServicePoolTest, Test2) {
  IOServicePool p("Test", kPoolSize);
  for (int k = 0; k < 1000; ++k) {
    p.Start();
    int item_size = kPoolSize * (rand() % 10 + 1);
    VLOG(2) << "item size: " << item_size;
    vector<int> v;
    v.resize(item_size, 0);
    for (int i = 0; i < item_size; ++i) {
      boost::function0<void> handler = boost::bind(
          &IOServicePoolTest::Inc2, this, &v[i]);
      p.get_io_service().dispatch(handler);
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
