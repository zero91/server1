/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */



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
