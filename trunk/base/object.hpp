#ifndef OBJECT_HPP_
#define OBJECT_HPP_
#include "boost/enable_shared_from_this.hpp"
class Object : public boost::enable_shared_from_this<Object> {
 public:
  virtual ~Object() {
  }
};

template <typename T>
class ObjectT : public Object {
 public:
  ObjectT(const shared_ptr<T> &t) : t_(t) {
  }
  static shared_ptr<Object> Create(shared_ptr<T> t) {
    return shared_ptr<Object>(new ObjectT(t));
  }
 private:
  shared_ptr<T> t_;
};
#endif  // OBJECT_HPP_
