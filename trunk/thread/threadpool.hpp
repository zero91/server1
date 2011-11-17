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



#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include "base/base.hpp"
#include "base/executor.hpp"
#include "thread/pcqueue.hpp"
#include <glog/logging.h>
class ThreadPool : public boost::noncopyable, public Executor {
 public:
  ThreadPool(const string name, int size) : name_(name), size_(size), timeout_(kDefaultTimeout) {
  }
  ~ThreadPool() {
    if (IsRunning()) {
      LOG(WARNING) << "Stop thread pool: " << name_ << " in destructor";
      Stop();
      LOG(WARNING) << "Stopped thread pool: " << name_ << " in destructor";
    }
  }
  void Start() {
    VLOG(1) << name() << " Start, size:" << size_;
    boost::mutex::scoped_lock locker(run_mutex_);
    if (!threads_.empty()) {
      VLOG(1) << name() << " Running.";
      return;
    }
    for (int i = 0; i < size_; ++i) {
      boost::shared_ptr<boost::thread> t(new boost::thread(
          boost::bind(&ThreadPool::Loop, this, i, name_.empty() ? "NoName" : strdup(name_.c_str()))));
      threads_.push_back(t);
    }
  }
  bool IsRunning() {
    return threads_.size() > 0;
  }
  const string name() const {
    return name_;
  }
  void set_stop_timeout(int timeout) {
    timeout_ = timeout;
  }
  int size() const {
    return size_;
  }
  void Stop() {
    VLOG(1) << name() << " Stop.";
    boost::mutex::scoped_lock locker(run_mutex_);
    if (threads_.empty()) {
      VLOG(1) << name() << " already stop.";
      return;
    }
    for (int i = 0; i < threads_.size(); ++i) {
      pcqueue_.Push(boost::function0<void>());
    }
    for (int i = 0; i < threads_.size(); ++i) {
      bool ret = threads_[i]->timed_join(boost::posix_time::seconds(timeout_));
      VLOG(2) << "Join threads: " << i << " ret: " << ret;
    }
    VLOG(1) << name() << " Stopped";
    threads_.clear();
  }
  void PushTask(const boost::function0<void> &t) {
    if (t.empty()) {
      LOG(WARNING) << name() <<  " can't push null task.";
      return;
    }
    pcqueue_.Push(t);
  }
  void Run(const boost::function0<void> &f) {
    PushTask(f);
  }
 private:
  static const int kDefaultTimeout = 60;
  void Loop(int i, const char *pool_name) {
    VLOG(2) << pool_name << " worker " << i << " start.";
    while (1) {
      VLOG(2) << pool_name << " worker " << i << " wait task.";
      boost::function0<void> h = pcqueue_.Pop();
      if (h.empty()) {
        VLOG(2) << pool_name << " worker " << i << " get empty task, so break.";
        break;
      }
      VLOG(2) << pool_name << " woker " << i << " running task";
      h();
      VLOG(2) << pool_name << " woker " << i << " finish task";
    }
    VLOG(2) << pool_name << " woker " << i << " stop";
    delete pool_name;
  }
  int timeout_;
  vector<boost::shared_ptr<boost::thread> > threads_;
  int size_;
  boost::mutex run_mutex_;
  PCQueue<boost::function0<void> > pcqueue_;
  string name_;
};
#endif  // THREAD_POOL_HPP_
