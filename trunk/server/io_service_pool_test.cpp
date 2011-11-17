/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */



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
    VLOG(0) << "Inc2 call, cnt: " << *cnt;
    sleep(rand() % 3 + 1);
    *cnt = 0xbeef;
  }
 protected:
  static const int kPoolSize = 100;
  static const int kThreadSize = 200;
};

TEST_F(IOServicePoolTest, Test1) {
  IOServicePool p("Test", kPoolSize, kThreadSize);
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
    VLOG(0) << "thread stopped" << k;
  }
}

TEST_F(IOServicePoolTest, Test2) {
  IOServicePool p("Test", kPoolSize, kThreadSize);
  for (int k = 0; k < 100; ++k) {
    p.Start();
    int item_size = kPoolSize * (rand() % 10 + 1);
    VLOG(2) << "item size: " << item_size;
    vector<int> v;
    v.resize(item_size, 0);
    for (int i = 0; i < item_size; ++i) {
      boost::function0<void> handler = boost::bind(
          &IOServicePoolTest::Inc2, this, &v[i]);
      p.get_io_service().post(handler);
    }
    p.Stop();
    for (int i = 0; i < item_size; ++i) {
      EXPECT_EQ(v[i], 0xbeef);
    }
    VLOG(0) << "thread stopped" << k;
  }
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
