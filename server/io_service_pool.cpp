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
    threadpool_(name + ".ThreadPool", num_threads_) {
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
