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
  }

  void reserve(int capacity) {
    size_ = 0;
    capacity_ = capacity;
    t_.reset(new T[capacity_ + 1]);
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
