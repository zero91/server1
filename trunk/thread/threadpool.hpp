// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

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
    CHECK(!IsRunning());
  }
  void Start() {
    VLOG(1) << name() << " Start.";
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
