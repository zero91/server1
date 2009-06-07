// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include "server/io_service_pool.hpp"
#include <boost/bind.hpp>
#include <glog/logging.h>
IOServicePool::IOServicePool(
    const string &name,
    size_t num_io_services,
    size_t num_threads)
  : name_(name),
    num_io_services_(num_io_services),
    num_threads_(num_threads),
    next_io_service_(0),
    threadpool_(name + ".ThreadPool", num_threads) {
  CHECK_GE(num_threads, num_io_services);
}

void IOServicePool::Start() {
  boost::mutex::scoped_lock locker(mutex_);
  if (!work_.empty()) {
    LOG(WARNING) << "IOServicePool already running";
    return;
  }
  CHECK(io_services_.empty());
  for (size_t i = 0; i < num_io_services_; ++i) {
    boost::shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
    io_services_.push_back(io_service);
  }
  // Give all the io_services work to do so that their run() functions will not
  // exit until they are explicitly stopped.
  work_.clear();
  work_.reserve(num_threads_);
  int k = 0;
  for (int i = 0; i < num_threads_; ++i) {
    boost::shared_ptr<boost::asio::io_service> io_service = io_services_[k];
    k = (k + 1) % num_io_services_;
    boost::shared_ptr<boost::asio::io_service::work> worker(new boost::asio::io_service::work(*io_service));
    work_.push_back(worker);
    threadpool_.PushTask(boost::bind(&boost::asio::io_service::run, io_service.get()));
  }
  threadpool_.Start();
}

void IOServicePool::Stop() {
  boost::mutex::scoped_lock locker(mutex_);
  if (work_.empty()) {
    LOG(WARNING) << "IOServicePool already stop";
    return;
  }
  /*
  for (size_t i = 0; i < work_.size(); ++i) {
    work_[i].reset();
  }
  */
  for (size_t i = 0; i < io_services_.size(); ++i) {
    io_services_[i]->stop();
  }
  threadpool_.Stop();
  work_.clear();
  io_services_.clear();
}

boost::asio::io_service &IOServicePool::get_io_service() {
  boost::mutex::scoped_lock locker(mutex_);
  // Use a round-robin scheme to choose the next io_service to use.
  boost::asio::io_service &io_service = *io_services_[next_io_service_];
  ++next_io_service_;
  if (next_io_service_ == io_services_.size())
    next_io_service_ = 0;
  return io_service;
}
