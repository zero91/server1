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
#ifndef NOTIFIER_HPP_
#define NOTIFIER_HPP_
#include "base/base.hpp"
#include <boost/bind.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
class Notifier : public boost::enable_shared_from_this<Notifier>, public boost::noncopyable {
 public:
  Notifier(const string name, int initial_count = 1) : name_(name), count_(initial_count), notified_(false) {
  }
  const boost::function0<void> notify_handler() {
    return boost::bind(&Notifier::Notify, shared_from_this());
  }
  void Notify() {
    Dec(1);
  }
  void Inc(int cnt) {
    Dec(-1 * cnt);
  }
  void Dec(int cnt) {
    boost::mutex::scoped_lock locker(mutex_);
    if (cnt > 0 && count_ >= cnt) {
      count_ -= cnt;
    } else if (cnt < 0) {
      count_ -= cnt;
    }
    VLOG(2) << name_ << " : " << "count: " << count_;
    if (count_ == 0) {
      notify_.notify_all();
      notified_ = true;
      VLOG(2) << name_ << " : " << "Notifed";
    }
    CHECK_GE(count_, 0);
  }
  // Return true when notified, otherwise return false.
  bool Wait() {
    return Wait(LONG_MAX);
  }
  bool Wait(int timeout_ms) {
    boost::mutex::scoped_lock locker(mutex_);
    while (!notified_) {
      VLOG(2) << name_ << " : " << "Wait";
      bool ret = notify_.timed_wait(
          locker, boost::posix_time::milliseconds(timeout_ms));
      VLOG(2) << name_ << " : " << "Wait return";
      return ret;
    }
    return true;
  }
  int count() const {
    return count_;
  }
 private:
  int count_;
  boost::mutex mutex_;
  boost::condition notify_;
  string name_;
  bool notified_;
};
#endif // NOTIFIER_HPP_
