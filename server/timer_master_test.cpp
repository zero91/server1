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
#include "server/timer_master.hpp"
#include <sstream>

struct TestTimer : public Timer,
  public boost::enable_shared_from_this<TestTimer> {
  bool period() const {
    return period_;
  }
  int timeout() const {
    return timeout_;
  }
  void Expired() {
    expired_ = true;
  }
  bool period_, expired_;
  int timeout_;
};


class TimerMasterTest : public testing::Test {
};

TEST_F(TimerMasterTest, Test0) {
  boost::shared_ptr<TestTimer> timer(new TestTimer);
  TimerMaster m;
  timer->period_ = true;
  timer->timeout_ = 2;
  timer->expired_ = false;
  m.Register(timer);
  m.Update(2);
  ASSERT_FALSE(timer->expired_);
  m.Update(3);
  ASSERT_TRUE(timer->expired_);
}

TEST_F(TimerMasterTest, Test1) {
  boost::shared_ptr<TestTimer> timer(new TestTimer);
  TimerMaster m;
  timer->period_ = true;
  timer->timeout_ = 1;
  timer->expired_ = false;
  m.Register(timer);
  for (int i = 2; i < 0xFFFF; ++i) {
    m.Update(i);
    ASSERT_TRUE(timer->expired_) << i;
    timer->expired_ = false;
  }
  for (int i = 0xFFFF; i < 0xFFFFF; ++i) {
    m.Update(i);
    ASSERT_TRUE(timer->expired_);
    timer->expired_ = false;
  }
}

TEST_F(TimerMasterTest, Test2) {
  vector<boost::shared_ptr<TestTimer> > timers;
  TimerMaster m;
  for (int i = 2; i < 0x10000; ++i) {
    boost::shared_ptr<TestTimer> timer(new TestTimer);
    timer->period_ = false;
    timer->timeout_ = i - 1;
    timer->expired_ = false;
    m.Register(timer);
    timers.push_back(timer);
  }
  for (int i = 2; i < 0x10000; ++i) {
    m.Update(i);
    ASSERT_TRUE(timers[i - 2]->expired_);
    timers[i - 2]->expired_ = false;
    VLOG(2) << i;
  }
  ASSERT_FALSE(timers[0]->expired_);
}

TEST_F(TimerMasterTest, Test3) {
  TimerMaster m;
  for (int i = 1; i < 0x10000; ++i) {
    boost::shared_ptr<TestTimer> timer(new TestTimer);
    timer->period_ = false;
    timer->timeout_ = i;
    timer->expired_ = false;
    m.Register(timer);
  }
  for (int i = 1; i < 0x10000; ++i) {
    m.Update(i);
  }
}

TEST_F(TimerMasterTest, Test4) {
  static const int magic[] = {
    1,
    2,
    5,
    255,
    254,
    256,
    0xFF00,
    0x1000,
    0xFF01,
    0xFFFF};
  int max_timeout = 0xFFFF;
  srand(time(0));
  for (int i = 0; i < arraysize(magic); ++i) {
    TimerMaster m;
    boost::shared_ptr<TestTimer> timer(new TestTimer);
    timer->period_ = true;
    timer->timeout_ = magic[i];
    timer->expired_ = false;
    m.Register(timer);
    m.Update(magic[i]);
    ASSERT_FALSE(timer->expired_) << magic[i];
    int t = magic[i] + rand() % 0xFFFFF + 1;
    LOG(INFO) << "Update: " << t;
    m.Update(t);
    LOG(INFO) << "Pass: " << t;
    ASSERT_TRUE(timer->expired_) << t;
  }
}

TEST_F(TimerMasterTest, Test5) {
  static const int magic[] = {
    1,
    2,
    5,
    255,
    254,
    256,
    0xFF00,
    0x1000,
    0xFF01,
    0xFFFF};
  int max_timeout = 0xFFFF;
  vector<boost::shared_ptr<TestTimer> > timers;
  TimerMaster m;
  for (int i = 0; i < arraysize(magic); ++i) {
    boost::shared_ptr<TestTimer> timer(new TestTimer);
    timer->period_ = true;
    timer->timeout_ = magic[i];
    timer->expired_ = false;
    m.Register(timer);
    timers.push_back(timer);
  }
  for (int i = 2; i < max_timeout; ++i) {
    m.Update(i);
    if (i > 0) {
      for (int j = 0; j < timers.size(); ++j) {
        if ((i - 1) % timers[j]->timeout_ == 0) {
          ASSERT_TRUE(timers[j]->expired_) << j;
          timers[j]->expired_ = false;
        }
      }
    }
  }
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
