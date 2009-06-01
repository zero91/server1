// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef PCQUEUE_HPP_
#define PCQUEUE_HPP_
#include <glog/logging.h>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
template <class Type>
class PCQueue : public boost::noncopyable, public boost::enable_shared_from_this<PCQueue<Type> > {
 public:
  PCQueue() {
  }
  Type Pop() {
    boost::mutex::scoped_lock locker(mutex_);
    while (queue_.empty()) {
      queue_not_empty_.wait(locker);
    }
    CHECK(!queue_.empty());
    Type t(queue_.front());
    queue_.pop_front();
    return t;
  }
  void Push(const Type &t) {
    boost::mutex::scoped_lock locker(mutex_);
    queue_.push_back(t);
    // Benchmark show notify_all is better than notify_one, 5.09ms vs 5.90
    // running file_transfer_client_test.
    // queue_not_empty_.notify_one();
    queue_not_empty_.notify_all();
  }
  int size() const {
    return queue_.size();
  }

 private:
  deque<Type> queue_;
  boost::mutex mutex_;
  boost::condition queue_not_empty_;
};
#endif  // PCQUEUE_HPP_


