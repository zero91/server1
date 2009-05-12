#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include "base/base.hpp"
#include "base/executor.hpp"
#include "thread/pcqueue.hpp"
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
    running_ = true;
    for (int i = 0; i < size_; ++i) {
      shared_ptr<boost::thread> t(new boost::thread(
          bind(&ThreadPool::Loop, shared_from_this())));
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
    running_ = false;
    for (int i = 0; i < threads_.size(); ++i) {
      shared_ptr<boost::function0<void> > exiter(
          new boost::function0<void>(boost::bind(
          &ThreadPool::Dummy, shared_from_this())));
      PushTask(exiter);
    }
    for (int i = 0; i < threads_.size(); ++i) {
      threads_[i]->timed_join(boost::posix_time::millisec(10));
    }
    threads_.clear();
  }
  template <typename Type>
  void PushTask(shared_ptr<Type> t) {
    pcqueue_.Push(t);
  }
  void Run(const boost::function0<void> &f) {
    shared_ptr<boost::function0<void> > executor(
        new boost::function0<void>(f));
    PushTask(executor);
  }
 private:
  void Dummy() {
  }
  void Loop() {
    while (running_) {
      (*pcqueue_.Pop().get())();
    }
  }
  PCQueue<boost::function0<void> > pcqueue_;
  vector<shared_ptr<boost::thread> > threads_;
  int size_;
  boost::mutex run_mutex_;
  bool running_;
};
#endif  // THREAD_POOL_HPP_
