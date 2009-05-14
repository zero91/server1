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
    queue_not_empty_.notify_one();
  }

 private:
  list<Type> queue_;
  boost::mutex mutex_;
  boost::condition queue_not_empty_;
};
#endif  // PCQUEUE_HPP_


