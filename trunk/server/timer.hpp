#ifndef TIMER_HPP_
#define TIMER_HPP_
#include "base/base.hpp"
#include "base/atomic.hpp"
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
// Threadsafe wrapper around the deadline_timer
class Timer : public boost::enable_shared_from_this<Timer> {
 public:
  Timer(boost::asio::io_service &ios, int timeout)
    : timer_(new boost::asio::deadline_timer(ios)), timeout_(timeout),
      intrusive_count_(0) {
  }
  // return true if success reschedule, otherwise, the timer is expired.
  template <typename WaitHandler>
  bool Wait(WaitHandler handler) {
    boost::mutex::scoped_lock locker(mutex_);
    if (timer_->expires_from_now(boost::posix_time::milliseconds(timeout_)) >= 0) {
      timer_->boost::asio::deadline_timer::async_wait(handler);
      return true;
    }
    return false;
  }
  void Cancel() {
    boost::mutex::scoped_lock locker(mutex_);
    timer_->cancel();
  }
 private:
  volatile int intrusive_count_;
  scoped_ptr<boost::asio::deadline_timer> timer_;
  boost::mutex mutex_;
  int timeout_;
  template <class T> friend void intrusive_ptr_add_ref(T *t);
  template <class T> friend void intrusive_ptr_release(T *t);
};
#endif  // TIMER_HPP_
