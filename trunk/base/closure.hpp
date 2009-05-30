// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef CLOSURE_HPP_
#define CLOSURE_HPP_
#include <boost/function.hpp>
class OnceClosure : public google::protobuf::Closure {
public:
 OnceClosure(const boost::function0<void> &f) : f_(f) {
 }
 void Run() {
   f_();
   delete this;
 }
private:
 boost::function0<void> f_;
};

class PermenantClosure : public google::protobuf::Closure {
public:
 PermenantClosure(const boost::function0<void> &f) : f_(f) {
 }
 void Run() {
   f_();
 }
private:
 boost::function0<void> f_;
};


inline google::protobuf::Closure *NewClosure(
    const boost::function0<void> &f) {
  return new OnceClosure(f);
}

inline google::protobuf::Closure *NewPermenantClosure(
    const boost::function0<void> &f) {
  return new PermenantClosure(f);
}

class ScopedClosure {
 public:
  ScopedClosure(google::protobuf::Closure *closure) : closure_(closure) {
  }
  ~ScopedClosure() {
    closure_->Run();
  }
 private:
  google::protobuf::Closure *closure_;
};
#endif  // CLOSURE_HPP_
