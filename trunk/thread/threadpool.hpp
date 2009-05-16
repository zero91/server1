#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include "base/base.hpp"
#include "base/executor.hpp"
#include "thread/pcqueue.hpp"
#include <boost/thread/tss.hpp>
#include <glog/logging.h>
class ThreadPool : public boost::noncopyable, public Executor {
 public:
  ThreadPool(const string name, int size) : name_(name), size_(size) {
  }
  void Start() {
    VLOG(2) << name() << " Start.";
    boost::mutex::scoped_try_lock locker(run_mutex_);
    if (threads_.size() > 0) {
      VLOG(2) << name() << " Running.";
      return;
    }
    for (int i = 0; i < size_; ++i) {
      boost::shared_ptr<boost::thread> t(new boost::thread(&ThreadPool::Loop, this, i));
      threads_.push_back(t);
    }
  }
  bool IsRunning() {
    return !threads_.empty();
  }
  const string name() const {
    return name_;
  }
  void Stop() {
    VLOG(2) << name() << " Stop.";
    boost::mutex::scoped_try_lock locker(run_mutex_);
    if (threads_.empty()) {
      VLOG(2) << name() << " already stop.";
      return;
    }
    for (int i = 0; i < threads_.size(); ++i) {
      pcqueue_.Push(boost::function0<void>());
    }
    for (int i = 0; i < threads_.size(); ++i) {
      threads_[i]->join();
      VLOG(2) << name() << " join thread: " << i;
    }
    threads_.clear();
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
    sigset_t mask;
    sigfillset(&mask); /* Mask all allowed signals */
    int rc = pthread_sigmask(SIG_SETMASK, &mask, NULL);
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
  vector<boost::shared_ptr<boost::thread> > threads_;
  int size_;
  boost::mutex run_mutex_;
  PCQueue<boost::function0<void> > pcqueue_;
  string name_;
};
#endif  // THREAD_POOL_HPP_
