#ifndef TIMER_HPP_
#define TIMER_HPP_
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
// Threadsafe wrapper around the deadline_timer
class Timer : public boost::asio::deadline_timer {
 public:
  typedef boost::asio::deadline_timer::time_type time_type;
  typedef  boost::asio::deadline_timer::duration_type duration_type;
  Timer(boost::asio::io_service &ios) : boost::asio::deadline_timer(ios) {
  }
  template <typename WaitHandler>
    void async_wait(WaitHandler handler) {
      boost::mutex::scoped_lock locker(mutex_);
      boost::asio::deadline_timer::async_wait(handler);
    }
  std::size_t cancel() {
    boost::mutex::scoped_lock locker(mutex_);
    return boost::asio::deadline_timer::cancel();
  }
  std::size_t cancel(boost::system::error_code& ec)
  {
    boost::mutex::scoped_lock locker(mutex_);
    return boost::asio::deadline_timer::cancel(ec);
  }
  time_type expires_at() {
    boost::mutex::scoped_lock locker(mutex_);
    return boost::asio::deadline_timer::expires_at();
  }
  std::size_t expires_at(const time_type& expiry_time,
                         boost::system::error_code& ec) {
    boost::mutex::scoped_lock locker(mutex_);
    return boost::asio::deadline_timer::expires_at(
        expiry_time, ec);
  }
  duration_type expires_from_now() {
    boost::mutex::scoped_lock locker(mutex_);
    return boost::asio::deadline_timer::expires_from_now();
  }
  std::size_t expires_from_now(const duration_type& expiry_time) {
    boost::mutex::scoped_lock locker(mutex_);
    return boost::asio::deadline_timer::expires_from_now(
        expiry_time);
  }
  std::size_t expires_from_now(const duration_type& expiry_time,
                               boost::system::error_code& ec) {
    boost::mutex::scoped_lock locker(mutex_);
    return boost::asio::deadline_timer::expires_from_now(
        expiry_time, ec);
  }
  void wait() {
    boost::mutex::scoped_lock locker(mutex_);
    return boost::asio::deadline_timer::wait();
  }
  void wait(boost::system::error_code& ec) {
    boost::mutex::scoped_lock locker(mutex_);
    return boost::asio::deadline_timer::wait(ec);
  }
 private:
  boost::mutex mutex_;
};
#endif  // TIMER_HPP_
