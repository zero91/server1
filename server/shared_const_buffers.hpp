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



#ifndef SHARED_CONST_BUFFERS_HPP_
#define SHARED_CONST_BUFFERS_HPP_
#include "base/base.hpp"
#include <boost/asio.hpp>
class SharedConstBuffers {
 private:
  struct Store {
    vector<const string *> data;
    ~Store() {
      for (int i = 0; i < data.size(); ++i) {
        VLOG(2) << "SharedConstBuffers Store delete: " << data[i];
        delete data[i];
      }
      data.clear();
    }
  };
 public:
  // Implement the ConstBufferSequence requirements.
  typedef boost::asio::const_buffer value_type;
  typedef vector<boost::asio::const_buffer>::const_iterator const_iterator;
  const const_iterator begin() const {
    return buffer_.begin() + start_;
  }
  const const_iterator end() const {
    return buffer_.end();
  }
  SharedConstBuffers() : start_(0), store_(new Store) {
  }
  void push(const string *data) {
    VLOG(2) << "SharedConstBuffers push: " << data;
    store_->data.push_back(data);
    buffer_.push_back(boost::asio::const_buffer(data->c_str(), data->size()));
  }
  void clear() {
    VLOG(2) << "Clear SharedConstBuffers";
    buffer_.clear();
    store_.reset(new Store);
    start_ = 0;
  }
  bool empty() const {
    return (start_ == buffer_.size());
  }
  int size() const {
    int s = 0;
    for (int i = start_; i < buffer_.size(); ++i) {
      const int bsize = boost::asio::buffer_size(buffer_[i]);
      s += bsize;
    }
    return s;
  }
  void consume(int size) {
    for (int i = start_; i < buffer_.size(); ++i) {
      const int bsize = boost::asio::buffer_size(buffer_[i]);
      if (size > bsize) {
        size -= bsize;
      } else if (size == bsize) {
        start_ = i + 1;
        return;
      } else {
        buffer_[i] = buffer_[i] + size;
        start_ = i;
        return;
      }
    }
    start_ = buffer_.size();
  }
 private:
  vector<boost::asio::const_buffer> buffer_;
  boost::shared_ptr<Store> store_;
  int start_;
};
#endif // SHARED_CONST_BUFFERS_HPP_
