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



#ifndef PCQUEUE_HPP_
#define PCQUEUE_HPP_
#include <glog/logging.h>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <deque>
template <class Type>
class PCQueue : public boost::noncopyable, public boost::enable_shared_from_this<PCQueue<Type> > {
 public:
  PCQueue() {
  }
  Type Pop() {
    boost::mutex::scoped_lock locker(mutex_);
    while (queue_.empty()) {
      queue_not_empty_.wait(locker);
    }
    CHECK(!queue_.empty());
    Type t(queue_.front());
    queue_.pop_front();
    return t;
  }
  void Push(const Type &t) {
    boost::mutex::scoped_lock locker(mutex_);
    queue_.push_back(t);
    // Benchmark show notify_all is better than notify_one, 5.09ms vs 5.90
    // running file_transfer_client_test.
    // queue_not_empty_.notify_one();
    queue_not_empty_.notify_all();
  }
  int size() const {
    return queue_.size();
  }

 private:
  deque<Type> queue_;
  boost::mutex mutex_;
  boost::condition queue_not_empty_;
};
#endif  // PCQUEUE_HPP_


