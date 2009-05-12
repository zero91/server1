#ifndef PCQUEUE_HPP_
#define PCQUEUE_HPP_
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
template <class Type>
class PCQueue : public boost::noncopyable, boost::enable_shared_from_this<PCQueue<Type> > {
 public:
  PCQueue() {
  }
  shared_ptr<Type> Pop() {
    boost::mutex::scoped_lock locker(mutex_);
    if (queue_.empty()) {
      queue_not_empty_.wait(locker);
    }
    shared_ptr<Type> t = queue_.front();
    queue_.pop_front();
    return t;
  }
  void Push(shared_ptr<Type> t) {
    boost::mutex::scoped_lock locker(mutex_);
    if (queue_.empty()) {
      queue_not_empty_.notify_one();
    }
    queue_.push_back(t);
  }
 private:
  list<shared_ptr<Type> > queue_;
  boost::mutex mutex_;
  boost::condition queue_not_empty_;
};
#endif  // PCQUEUE_HPP_


