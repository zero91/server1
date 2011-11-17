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



#ifndef HASH_HPP_
#define HASH_HPP_
#include <ext/hash_map>
#include "base/base.hpp"
#include "base/basictypes.hpp"
extern "C" uint64 hash8(const uint8 *, uint64, uint64);
inline uint64 hash8(const string &x) {
  return hash8(reinterpret_cast<const uint8*>(x.c_str()),
               static_cast<uint64>(x.size()), 0xbeef);
}

# if defined(_MSC_VER)
// MSVC's stl implementation doesn't have this hash struct.
template <typename T>
struct hash {
  size_t operator()(const T& t) const;
};
#else
namespace __gnu_cxx {
template<> struct hash<void*> {
  size_t operator()(void* x) const { return reinterpret_cast<size_t>(x); }
};
template<> struct hash<int64> {
  size_t operator()(int64 x) const { return static_cast<size_t>(x); }
};

template<> struct hash<uint64> {
  size_t operator()(uint64 x) const { return static_cast<size_t>(x); }
};

template<> struct hash<string> {
  size_t operator()(const string &x) const {
    return static_cast<size_t>(hash8(x));
  }
};
template<class T> struct hash<T*> {
    size_t operator()(T *x) const { return reinterpret_cast<size_t>(x); }
};
template<class T> struct hash<boost::shared_ptr<T> > {
    size_t operator()(const boost::shared_ptr<T> &x) const {
      return reinterpret_cast<size_t>(x.get());
    }
};
}
#endif

#endif  // HASH_HPP_
