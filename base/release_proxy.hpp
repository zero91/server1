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
#ifndef RELEASE_PROXY_HPP_
#define RELEASE_PROXY_HPP_

#include "base/base.hpp"
#include <boost/thread/shared_mutex.hpp>
#include <boost/bind.hpp>

// to proxy to a handler when the hander is still available.
class ReleaseProxy : public boost::enable_shared_from_this<ReleaseProxy> {
 public:
  ReleaseProxy() : valid_(true) {
  }

  template <typename T>
  inline boost::function0<typename T::result_type> proxy(const T &h);
  void Invalid() {
    mutex_.lock();
    valid_ = false;
    mutex_.unlock();
  }
 private:
  template <typename T, typename ResultType>
  struct Call {
    static inline ResultType Run(boost::shared_ptr<ReleaseProxy> proxy, const T h);
  };
  bool valid_;
  boost::shared_mutex mutex_;
  template <typename T, typename ResultType> friend class Call;
};

template <typename T, typename ResultType>
ResultType ReleaseProxy::Call<T, ResultType>::Run(
    boost::shared_ptr<ReleaseProxy> proxy,
    const T h) {
  proxy->mutex_.lock_shared();
  if (proxy->valid_) {
    ResultType t = h();
    proxy->mutex_.unlock_shared();
    return t;
  }
  proxy->mutex_.unlock_shared();
  return ResultType(0);
}

template <typename T>
struct ReleaseProxy::Call<T, void> {
  static inline void Run(boost::shared_ptr<ReleaseProxy> proxy, const T h) {
    proxy->mutex_.lock_shared();
    if (proxy->valid_) {
      h();
      proxy->mutex_.unlock_shared();
      return;
    }
    proxy->mutex_.unlock_shared();
  }
};

template <typename T>
boost::function0<typename T::result_type> ReleaseProxy::proxy(
    const T &h) {
  return boost::bind(&ReleaseProxy::Call<T, typename T::result_type>::Run, shared_from_this(), h);
}
#endif  // RELEASE_PROXY_HPP_
