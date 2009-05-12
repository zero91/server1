#ifndef EXECUTOR_HPP_
#define EXECUTOR_HPP_
#include <boost/function.hpp>
class Executor {
 public:
  virtual void Run(const boost::function0<void> &f) {
    f();
  }
};
#endif  // EXECUTOR_HPP_


