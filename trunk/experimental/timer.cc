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

void timeout(boost::asio::deadline_timer *timer, const boost::system::error_code &e) {
  std::cout << "timeout, e: " << e.message() << std::endl;
  timer->async_wait(boost::bind(timeout, timer, _1));
}

int main(int argc, char **argv) {
  std::cout << "before ioservice" << std::endl;
  boost::asio::io_service io_service;
  std::cout << "before work" << std::endl;
  boost::function0<void> h(boost::bind(
      &boost::asio::io_service::run, &io_service));
  std::cout << "before thread" << std::endl;
  boost::thread t(h);
  boost::asio::io_service::work work(io_service);
  std::cout << "thread" << std::endl;
  std::cout << "work" << std::endl;
  boost::asio::deadline_timer timer(io_service);
  timer.expires_from_now(boost::posix_time::seconds(2));
  std::cout << "before asynwait" << std::endl;
  timer.async_wait(timeout);
  sleep(2);
  // timer.expires_from_now(boost::posix_time::seconds(10));
  //timer.async_wait(strand.wrap(boost::bind(timeout, &timer, _1)));
  // timer.expires_from_now(boost::posix_time::seconds(10));
  // timer.async_wait(strand.wrap(boost::bind(timeout, &timer, _1)));
  // timer.cancel();
  std::cout << "t join" << std::endl;
  t.join();
  return 0;
}
