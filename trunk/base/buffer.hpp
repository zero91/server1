/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */



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
