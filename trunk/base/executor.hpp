// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

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


