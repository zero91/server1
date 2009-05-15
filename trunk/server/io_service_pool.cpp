#include "server/io_service_pool.hpp"
#include <boost/bind.hpp>
#include <glog/logging.h>
IOServicePool::IOServicePool(size_t pool_size)
  : pool_size_(pool_size),
    next_io_service_(0), threadpool_(new ThreadPool(pool_size)) {
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
    io_services_.push_back(boost::asio::io_service());
    work_.push_back(boost::asio::io_service::work(io_services_.back()));
    threadpool_->PushTask(boost::bind(&boost::asio::io_service::run, &io_services_.back()));
  }
  threadpool_->Start();
}

void IOServicePool::Stop() {
  boost::mutex::scoped_lock locker(mutex_);
  if (work_.empty()) {
    LOG(WARNING) << "IOServicePool already stop";
    return;
  }
  for (size_t i = 0; i < work_.size(); ++i) {
    work_[i].reset();
//    io_services_[i]->stop();
  }
  threadpool_->Stop();
  work_.clear();
  io_services_.clear();
}

boost::asio::io_service &IOServicePool::get_io_service() {
  boost::mutex::scoped_lock locker(mutex_);
  // Use a round-robin scheme to choose the next io_service to use.
  boost::asio::io_service &io_service = io_services_[next_io_service_];
  ++next_io_service_;
  if (next_io_service_ == io_services_.size())
    next_io_service_ = 0;
  return io_service;
}
