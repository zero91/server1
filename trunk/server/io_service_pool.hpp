#ifndef NET2_IO_SERVICE_POOL_HPP_
#define NET2_IO_SERVICE_POOL_HPP_
#include "base/base.hpp"
#include <boost/thread.hpp>
#include "thread/threadpool.hpp"
/// A pool of io_service objects.
class IOServicePool : private boost::noncopyable {
public:
  /// Construct the io_service pool.
  explicit IOServicePool(const string &name, size_t pool_size);
  ~IOServicePool() {
    if (!work_.empty()) {
      Stop();
    }
  }

  /// Run all io_service objects in the pool.
  void Start();

  /// Stop all io_service objects in the pool.
  void Stop();

  /// Get an io_service to use.
  boost::asio::io_service &get_io_service();

private:

  /// The pool of io_services.
  vector<boost::shared_ptr<boost::asio::io_service> > io_services_;

  /// The work that keeps the io_services running.
  vector<boost::shared_ptr<boost::asio::io_service::work> > work_;

  int next_io_service_;

  ThreadPool threadpool_;
  boost::mutex mutex_;
  int pool_size_;
  string name_;
};
#endif // NET2_IO_SERVICE_POOL_HPP_
