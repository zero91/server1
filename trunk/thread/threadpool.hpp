#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include "base/base.hpp"
#include "base/executor.hpp"
#include "thread/pcqueue.hpp"
#include <boost/thread/tss.hpp>
#include <glog/logging.h>
class ThreadPool : public boost::noncopyable, public boost::enable_shared_from_this<ThreadPool>,
  public Executor {
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
    for (int i = 0; i < size_; ++i) {
      shared_ptr<boost::thread> t(new boost::thread(
          &ThreadPool::Loop, this, i));
      threads_.push_back(t);
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
      PushTask(boost::bind(&ThreadPool::Exit, this));
    }
    for (int i = 0; i < threads_.size(); ++i) {
      threads_[i]->join();
      VLOG(2) << "Join thread: " << i;
    }
    threads_.clear();
    loop_.reset();
  }
  void PushTask(const boost::function0<void> &t) {
    VLOG(2) << "PushTask";
    pcqueue_.Push(t);
  }
  void Run(const boost::function0<void> &f) {
    PushTask(f);
  }
 private:
  void Exit() {
    *loop_.get() = false;
  }
  void Loop(int i) {
    loop_.reset(new bool(true));
    boost::function0<void> h;
    VLOG(2) << "Thread pool worker " << i << " Start";
    while (*loop_.get()) {
      VLOG(2) << "Thread pool worker " << i << " wait task";
      pcqueue_.Pop()();
      VLOG(2) << "Thread pool worker " << i << " get task";
      VLOG(2) << "loop: " << *loop_.get();
    }
    VLOG(2) << "Thread pool worker " << i << " Stop";
  }
  boost::thread_specific_ptr<bool> loop_;
  vector<shared_ptr<boost::thread> > threads_;
  int size_;
  boost::mutex run_mutex_;
  PCQueue<boost::function0<void> > pcqueue_;
};
#endif  // THREAD_POOL_HPP_
