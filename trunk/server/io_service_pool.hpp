// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef NET2_IO_SERVICE_POOL_HPP_
#define NET2_IO_SERVICE_POOL_HPP_
#include "base/base.hpp"
#include <boost/thread.hpp>
#include "thread/threadpool.hpp"
#include "server/timer.hpp"
/// A pool of io_service objects.
class IOServicePool : private boost::noncopyable {
public:
  /// Construct the io_service pool.
  explicit IOServicePool(
      const string &name,
      size_t num_io_services,
      size_t num_threads);
  ~IOServicePool() {
    if (IsRunning()) {
      Stop();
    }
  }

  /// Run all io_service objects in the pool.
  void Start();

  /// Stop all io_service objects in the pool.
  void Stop();

  bool IsRunning() const {
    return !work_.empty();
  }

  /// Get an io_service to use.
  boost::asio::io_service &get_io_service();
private:

  /// The pool of io_services.
  vector<boost::shared_ptr<boost::asio::io_service> > io_services_;

  /// The work that keeps the io_services running.
  vector<boost::shared_ptr<boost::asio::io_service::work> > work_;

  int num_io_services_;
  int num_threads_;
  int next_io_service_;

  ThreadPool threadpool_;
  boost::mutex mutex_;
  string name_;
};
#endif // NET2_IO_SERVICE_POOL_HPP_
