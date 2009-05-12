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
  for (size_t i = 0; i < pool_size_; ++i) {
    IOServicePtr io_service(new boost::asio::io_service);
    work_ptr work(new boost::asio::io_service::work(*io_service));
    io_services_.push_back(io_service);
    work_.push_back(work);
    shared_ptr<boost::function0<void> > runner(
        new boost::function0<void>(boost::bind(&boost::asio::io_service::run, io_services_[i])));
    threadpool_->PushTask(runner);
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
    io_services_[i]->stop();
  }
  threadpool_->Stop();
  work_.clear();
  io_services_.clear();
}

IOServicePtr IOServicePool::get_io_service() {
  boost::mutex::scoped_lock locker(mutex_);
  // Use a round-robin scheme to choose the next io_service to use.
  IOServicePtr io_service = io_services_[next_io_service_];
  ++next_io_service_;
  if (next_io_service_ == io_services_.size())
    next_io_service_ = 0;
  return io_service;
}
