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



#ifndef ALLOCATOR_HPP_
#define ALLOCATOR_HPP_
#include "base/base.hpp"
#include <glog/logging.h>
// The class will destroy memory that is not freed when the class destroyed.
class Allocator {
 public:
  void* Allocate(std::size_t size) {
    void *p = ::operator new (size);
    VLOG(2) << "Allocated " << p << " size " << size;
    allocated_.push_back(p);
    return p;
  }

  void Deallocate(void *p) {
    VLOG(2) << "Allocated " << p << " deallocated";
    deallocated_.push_back(p);
  }
  ~Allocator() {
    VLOG(2) << "~Allocator: allocated: " << allocated_.size()
            << " deallocated: " << deallocated_.size();
    for (int i = 0; i < allocated_.size(); ++i) {
      int j = 0;
      for (; j < deallocated_.size(); ++j) {
        if (allocated_[i] == deallocated_[j]) {
          VLOG(2) << "free " << allocated_[i];
          ::operator delete (allocated_[i]);
          break;
        }
      }
      if (j == deallocated_.size()) {
        VLOG(2) << "undeallocated " << allocated_[i];
      }
    }
  }
 private:
  vector<void*> allocated_;
  vector<void*> deallocated_;
};
#endif  // ALLOCATOR_HPP_
