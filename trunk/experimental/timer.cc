#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <iostream>

void run() {
  std::cout << "run" << std::endl;
}

void timeout(const boost::system::error_code &e) {
  std::cout << "timeout, e: " << e.message() << std::endl;
}

int main(int argc, char **argv) {
  std::cout << "before ioservice" << std::endl;
  boost::asio::io_service io_service;
  boost::asio::io_service::strand strand(io_service);
  std::cout << "before timer" << std::endl;
  boost::asio::deadline_timer timer(io_service);
  std::cout << "before work" << std::endl;
  boost::function0<void> h(boost::bind(
      &boost::asio::io_service::run, &io_service));
  boost::function0<void> h2(boost::bind(run));
  std::cout << "before thread" << std::endl;
  boost::thread t(h);
  boost::asio::io_service::work work(io_service);
  std::cout << "thread" << std::endl;
  std::cout << "work" << std::endl;
  timer.expires_from_now(boost::posix_time::seconds(1));
  timer.async_wait(strand.wrap(timeout));
  timer.expires_from_now(boost::posix_time::seconds(10));
  timer.async_wait(strand.wrap(timeout));
  timer.expires_from_now(boost::posix_time::seconds(10));
  timer.async_wait(strand.wrap(timeout));
  timer.cancel();
  std::cout << "t join" << std::endl;
  t.join();
  return 0;
}
