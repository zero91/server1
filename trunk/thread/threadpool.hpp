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
  ThreadPool(const string name, int size) : name_(name), size_(size) {
  }
  void Start() {
    VLOG(1) << name() << " Start.";
    boost::mutex::scoped_try_lock locker(run_mutex_);
    if (threads_.get() != NULL) {
      VLOG(1) << name() << " Running.";
      return;
    }
    threads_.reset(new boost::thread_group);
    for (int i = 0; i < size_; ++i) {
      threads_->create_thread(boost::bind(&ThreadPool::Loop, this, i));
    }
  }
  bool IsRunning() {
    return threads_.get() && threads_->size() > 0;
  }
  const string name() const {
    return name_;
  }
  int size() const {
    return size_;
  }
  void Stop() {
    VLOG(1) << name() << " Stop.";
    boost::mutex::scoped_try_lock locker(run_mutex_);
    if (threads_.get() == NULL) {
      VLOG(1) << name() << " already stop.";
      return;
    }
    for (int i = 0; i < threads_->size(); ++i) {
      pcqueue_.Push(boost::function0<void>());
    }
    threads_->join_all();
    VLOG(1) << "Stopped";
    threads_.reset();
  }
  void PushTask(const boost::function0<void> &t) {
    if (t.empty()) {
      LOG(WARNING) << name() <<  " can't push null task.";
      return;
    }
    VLOG(2) << name() << " push task.";
    pcqueue_.Push(t);
  }
  void Run(const boost::function0<void> &f) {
    PushTask(f);
  }
 private:
  void Loop(int i) {
    VLOG(2) << name() << " worker " << i << " start.";
    while (1) {
      try {
        VLOG(2) << name() << " worker " << i << " wait task.";
        boost::function0<void> h = pcqueue_.Pop();
        if (h.empty()) {
          VLOG(2) << name() << " worker " << i << " get empty task, so break.";
          break;
        }
        VLOG(2) << name() << " woker " << i << " running task";
        h();
        VLOG(2) << name() << " woker " << i << " finish task";
      } catch (std::exception e) {
        VLOG(2) << name() << " woker " << i << " catch exception " << e.what();
      }
    }
    VLOG(2) << name() << " woker " << i << " stop";
  }
  boost::scoped_ptr<boost::thread_group> threads_;
  int size_;
  boost::mutex run_mutex_;
  PCQueue<boost::function0<void> > pcqueue_;
  string name_;
};
#endif  // THREAD_POOL_HPP_
