#include "server/io_service_pool.hpp"
#include <boost/thread.hpp>
#include <boost/bind.hpp>
IOServicePool::IOServicePool(size_t pool_size)
  : next_io_service_(0) {
  if (pool_size == 0)
    throw runtime_error("IOServicePool size is 0");

  // Give all the io_services work to do so that their run() functions will not
  // exit until they are explicitly stopped.
  for (size_t i = 0; i < pool_size; ++i) {
    IOServicePtr io_service(new asio::io_service);
    work_ptr work(new asio::io_service::work(*io_service));
    io_services_.push_back(io_service);
    work_.push_back(work);
  }
}

void IOServicePool::Run() {
  // Create a pool of threads to run all of the io_services.
  vector<shared_ptr<thread> > threads;
  for (size_t i = 0; i < io_services_.size(); ++i) {
    shared_ptr<thread> t(new thread(
        bind(&asio::io_service::run, io_services_[i])));
    threads.push_back(t);
  }

  // Wait for all threads in the pool to exit.
  for (size_t i = 0; i < threads.size(); ++i)
    threads[i]->join();
}

void IOServicePool::Stop() {
  // Explicitly stop all io_services.
  for (size_t i = 0; i < io_services_.size(); ++i)
    io_services_[i]->stop();
}

IOServicePtr IOServicePool::get_io_service() {
  // Use a round-robin scheme to choose the next io_service to use.
  IOServicePtr io_service = io_services_[next_io_service_];
  ++next_io_service_;
  if (next_io_service_ == io_services_.size())
    next_io_service_ = 0;
  return io_service;
}
