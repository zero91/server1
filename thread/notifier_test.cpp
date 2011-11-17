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
  static const int kItemSize = 20;
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

TEST_F(NotifierTest, TestWaitNumber) {
  ThreadPool p1("TestWait", kPoolSize);
  ThreadPool p2("TestNotifier", kPoolSize);
  const int kInitialNumber = 100;
  vector<boost::shared_ptr<Notifier> > ns;
  vector<int> v;
  v.resize(kItemSize, 0);
  p1.Start();
  for (int k = 0; k < kItemSize; ++k) {
    boost::shared_ptr<Notifier> n(new Notifier("Test", kInitialNumber));
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
  for (int i = 0; i < kInitialNumber; ++i) {
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

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
