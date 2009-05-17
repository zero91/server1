// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

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
