#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include "base/base.hpp"
#include "base/executor.hpp"
#include "thread/pcqueue.hpp"
#include <boost/thread/tss.hpp>
#include <glog/logging.h>
class ThreadPool : public boost::noncopyable, public Executor {
 public:
  ThreadPool(int size) : size_(size) {
  }
  void Start() {
    LOG(INFO) << "Start threadpool";
    boost::mutex::scoped_try_lock locker(run_mutex_);
    if (threads_.size() > 0) {
      LOG(INFO) << "ThreadPool Running";
      return;
    }
    threads_.reserve(size_);
    for (int i = 0; i < size_; ++i) {
      threads_.push_back(boost::thread(&ThreadPool::Loop, this, i));
    }
  }
  void Stop() {
    LOG(INFO) << "Stop threadpool";
    boost::mutex::scoped_try_lock locker(run_mutex_);
    if (threads_.empty()) {
      LOG(INFO) << "ThreadPool already stop";
      return;
    }
    for (int i = 0; i < threads_.size(); ++i) {
      PushTask(boost::function0<void>());
    }
    for (int i = 0; i < threads_.size(); ++i) {
      threads_[i].join();
      VLOG(2) << "Join thread: " << i;
    }
    threads_.clear();
  }
  void PushTask(const boost::function0<void> &t) {
    VLOG(2) << "PushTask";
    pcqueue_.Push(t);
  }
  void Run(const boost::function0<void> &f) {
    PushTask(f);
  }
 private:
  void Loop(int i) {
    VLOG(2) << "Thread pool worker " << i << " Start";
    while (1) {
      VLOG(2) << "Thread pool worker " << i << " wait task";
      boost::function0<void> h = pcqueue_.Pop();
      if (h.empty()) {
        VLOG(2) << "Thread pool worker " << i << " get empty task, so break";
        break;
      }
      h();
    }
    VLOG(2) << "Thread pool worker " << i << " Stop";
  }
  vector<boost::thread> threads_;
  int size_;
  boost::mutex run_mutex_;
  PCQueue<boost::function0<void> > pcqueue_;
};
#endif  // THREAD_POOL_HPP_
