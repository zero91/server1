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



#ifndef CLOSURE_HPP_
#define CLOSURE_HPP_
#include <glog/logging.h>
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
    if (closure_) {
      closure_->Run();
    } else {
      VLOG(2) << "Closure is NULL";
    }
  }
 private:
  google::protobuf::Closure *closure_;
};
#endif  // CLOSURE_HPP_
