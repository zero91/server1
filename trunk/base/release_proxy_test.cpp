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
#include <glog/logging.h>
#include "base/release_proxy.hpp"
#include "thread/threadpool.hpp"
#include <sstream>

struct TestItem {
  TestItem() : p (new ReleaseProxy) {
  }
  ~TestItem() {
    p->Invalid();
  }
  void Call() {
    cnt = 0;
  }
  int cnt;
  boost::shared_ptr<ReleaseProxy> p;
};

class ReleaseProxyTest : public testing::Test {
 public:
 protected:
  static const int kPoolSize = 200;
  static const int kItemSize = 100000;
};

TEST_F(ReleaseProxyTest, Test1) {
  ThreadPool p1("TestWait", kPoolSize);
  p1.Start();
  for (int k = 0; k < kItemSize; ++k) {
    TestItem item;
    boost::function0<void> h = boost::bind(&TestItem::Call, &item);
    p1.PushTask(item.p->proxy(h));
  }
  p1.Stop();
}

void DeleteItem(TestItem *item) {
  delete item;
}

TEST_F(ReleaseProxyTest, Test2) {
  ThreadPool p1("TestWait", kPoolSize);
  p1.Start();
  for (int k = 0; k < kItemSize; ++k) {
    TestItem *item = new TestItem;
    boost::function0<void> h = boost::bind(&TestItem::Call, item);
    p1.PushTask(item->p->proxy(h));
    p1.PushTask(boost::bind(&DeleteItem, item));
  }
  p1.Stop();
}


TEST_F(ReleaseProxyTest, Test3) {
  ThreadPool p1("TestWait", kPoolSize);
  p1.Start();
  for (int k = 0; k < kItemSize; ++k) {
    TestItem *item = new TestItem;
    boost::function0<void> h = boost::bind(&TestItem::Call, item);
    boost::function0<void> q1 = item->p->proxy(h);
    boost::function0<void> h2 = boost::bind(
        &TestItem::Call, item);
    boost::function0<void> q2 = item->p->proxy(h2);
    p1.PushTask(q1);
    p1.PushTask(q2);
    p1.PushTask(q2);
    p1.PushTask(q2);
    p1.PushTask(q2);
    p1.PushTask(q2);
    p1.PushTask(boost::bind(&DeleteItem, item));
    p1.PushTask(q2);
    p1.PushTask(q2);
    p1.PushTask(q2);
  }
  p1.Stop();
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
