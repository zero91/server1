#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <iostream>

void timeout(boost::shared_mutex *mutex, int i) {
  boost::shared_lock<boost::shared_mutex> locker(*mutex);
  std::cout << i << std::endl;
  timeout(mutex, i + 1);
}

int main(int argc, char **argv) {
  std::cout << "before timeout" << std::endl;
  boost::shared_mutex mutex;
//  mutex.lock_shared();
  //mutex.lock_upgrade();
  std::cout << "0" << std::endl;
  boost::shared_lock<boost::shared_mutex> shared(mutex);
  boost::upgrade_lock<boost::shared_mutex> locker(shared);
  std::cout << "1" << std::endl;
  std::cout << "2" << std::endl;
  boost::upgrade_to_unique_lock<boost::shared_mutex> lock3(locker);
  boost::shared_lock<boost::shared_mutex> shared2(mutex);
  //mutex.unlock_shared();
  std::cout << "3" << std::endl;
  std::cout << "endl" << std::endl;
  return 0;
}
