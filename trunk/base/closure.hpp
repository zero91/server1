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
#endif  // CLOSURE_HPP_
