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
#include "thread/notifier.hpp"
#include "thread/threadpool.hpp"
#include <sstream>

class NotifierTest : public testing::Test {
 public:
  void Wait(const boost::function0<bool> h,
           int *cnt) {
    VLOG(2) << "Inc call, cnt: " << *cnt << " wait";
    if (!h()) {
      return;
    }
    VLOG(2) << "Inc call, cnt: " << *cnt;
    *cnt = 0xbeef;
  }
 protected:
  static const int kPoolSize = 100;
  static const int kItemSize = 10000;
};

TEST_F(NotifierTest, TestWait) {
  ThreadPool p1("TestWait", kPoolSize);
  ThreadPool p2("TestNotifier", kPoolSize);
  vector<boost::shared_ptr<Notifier> > ns;
  vector<int> v;
  v.resize(kItemSize, 0);
  p1.Start();
  for (int k = 0; k < kItemSize; ++k) {
    boost::shared_ptr<Notifier> n(new Notifier("Test"));
    const boost::function0<bool> h = boost::bind(&Notifier::Wait, n);
    p1.PushTask(boost::bind(
        &NotifierTest::Wait, this,
        h, &v[k]));
    ns.push_back(n);
  }
  for (int i = 0; i < kItemSize; ++i) {
    EXPECT_EQ(v[i], 0);
  }
  p2.Start();
  for (int j = 0; j < 10; ++j) {
    for (int k = 0; k < kItemSize; ++k) {
      p2.PushTask(ns[k]->notify_handler());
    }
  }
  p2.Stop();
  p1.Stop();
  for (int i = 0; i < kItemSize; ++i) {
    EXPECT_EQ(v[i], 0xbeef);
  }
}

TEST_F(NotifierTest, TestWaitOut) {
  ThreadPool p1("TestNotifier", kPoolSize);
  vector<boost::shared_ptr<Notifier> > ns;
  vector<int> v;
  v.resize(kItemSize, 0);
  p1.Start();
  for (int k = 0; k < kItemSize; ++k) {
    boost::shared_ptr<Notifier> n(new Notifier("Test"));
    const boost::function0<bool> h = boost::bind(&Notifier::Wait, n, 1);
    p1.PushTask(boost::bind(
        &NotifierTest::Wait, this,
        h, &v[k]));
    ns.push_back(n);
  }
  p1.Stop();
  for (int i = 0; i < kItemSize; ++i) {
    EXPECT_EQ(v[i], 0);
  }
}

TEST_F(NotifierTest, TestDelete) {
  ThreadPool p1("TestNotifier", kPoolSize);
  vector<boost::shared_ptr<Notifier> > ns;
  vector<int> v;
  v.resize(kItemSize, 0);
  p1.Start();
  for (int k = 0; k < kItemSize; ++k) {
    boost::shared_ptr<Notifier> n(new Notifier("Test"));
    p1.PushTask(n->notify_handler());
  }
  for (int i = 0; i < kItemSize; ++i) {
    EXPECT_EQ(v[i], 0);
  }
  p1.Stop();
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
