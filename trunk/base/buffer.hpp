// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef BUFFER_H_
#define BUFFER_H_
#include "base.hpp"
template <class T>
class Buffer {
 public:
  Buffer() : size_(0), capacity_(0) {
  }

  Buffer(int capacity) {
    reserve(capacity);
  }

  void clear() {
    size_ = 0;
    capacity_ = 0;
    t_.reset();
  }

  void reserve(int capacity) {
    size_ = 0;
    t_.reset(new T[capacity + 1]);
    capacity_ = capacity;
  }

  T *data() const {
    return t_.get() + size_;
  }

  T *content() const {
    return t_.get();
  }

  void inc_size(int size) {
    size_ += size;
  }

  void push_back(const T &t) {
    *data() = t;
    inc_size(1);
  }

  int size() const {
    return capacity_ - size_;
  }

  bool full() const {
    return size() == 0;
  }

  int capacity() const {
    return capacity_;
  }
 private:
  scoped_array<T> t_;
  int size_, capacity_;
};
#endif  // BUFFER_H_
