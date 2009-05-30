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
IOServicePool::IOServicePool(const string &name, size_t pool_size)
  : name_(name), pool_size_(pool_size),
    next_io_service_(0), threadpool_(name + " threadpool", pool_size) {
  CHECK_GT(pool_size, 0);

}

void IOServicePool::Start() {
  boost::mutex::scoped_lock locker(mutex_);
  if (!work_.empty()) {
    LOG(WARNING) << "IOServicePool already running";
    return;
  }
  work_.clear();
  io_services_.clear();
  // Give all the io_services work to do so that their run() functions will not
  // exit until they are explicitly stopped.
  work_.reserve(pool_size_);
  io_services_.reserve(pool_size_);
  for (size_t i = 0; i < pool_size_; ++i) {
    boost::shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
    io_services_.push_back(io_service);
    boost::shared_ptr<boost::asio::io_service::work> worker(new boost::asio::io_service::work(*io_service));
    work_.push_back(worker);
    threadpool_.PushTask(boost::bind(&boost::asio::io_service::run, io_service));
  }
  threadpool_.Start();
}

void IOServicePool::Stop() {
  boost::mutex::scoped_lock locker(mutex_);
  if (work_.empty()) {
    LOG(WARNING) << "IOServicePool already stop";
    return;
  }
  for (size_t i = 0; i < work_.size(); ++i) {
    work_[i].reset();
  }
  boost::this_thread::yield();
  boost::this_thread::yield();
  boost::this_thread::yield();
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
