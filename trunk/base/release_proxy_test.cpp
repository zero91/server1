// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

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
