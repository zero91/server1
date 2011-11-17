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



#ifndef RAW_CONNECTION_HPP_
#define RAW_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/shared_const_buffers.hpp"
#include "boost/signals2/signal.hpp"
#include "boost/thread/shared_mutex.hpp"
#include "boost/function.hpp"
#include "boost/smart_ptr.hpp"
class RawConnectionStatus {
 public:
  typedef boost::shared_lock<boost::shared_mutex> Locker;
  RawConnectionStatus() : status_(0), intrusive_count_(0) {
  }
  ~RawConnectionStatus() {
    CHECK_EQ(intrusive_count_, 0);
  }
  bool reading() const {
    return status_ & READING;
  }

  bool writting() const {
    return status_ & WRITTING;
  }

  void set_reading() {
    atomic_or(&status_, READING);
  }

  void set_writting() {
    atomic_or(&status_, WRITTING);
  }

  void clear_reading() {
    atomic_and(&status_, ~READING);
  }

  void clear_writting() {
    atomic_and(&status_, ~WRITTING);
  }

  void set_closing() {
    atomic_or(&status_, CLOSING);
  }

  bool closing() const {
    return status_ & CLOSING;
  }

  bool idle() const {
    return status_ == IDLE;
  }

  int status() const {
    return status_;
  }
  boost::shared_mutex &mutex() {
    return mutex_;
  }
 private:
  enum InternalRawConnectionStatus {
    IDLE = 0x0,
    READING = 0x01,
    WRITTING = 0x01 << 1,
    CLOSING = 0x01 << 2,
  };
  volatile int status_;
  volatile int intrusive_count_;
  template <class T> friend void intrusive_ptr_add_ref(T *t);
  template <class T> friend void intrusive_ptr_release(T *t);
  boost::shared_mutex mutex_;
};

class Connection;
class RawConnection : public boost::noncopyable {
 private:
  typedef RawConnectionStatus::Locker Locker;
 public:
  typedef boost::intrusive_ptr<RawConnectionStatus> StatusPtr;
  RawConnection(const string &name,
                boost::shared_ptr<Connection> connection);
  void Disconnect(StatusPtr status, bool async);
  bool ScheduleWrite(StatusPtr status);
  // The push will take the ownership of the data
  template <typename T>
  inline bool PushData(const T &data) {
    boost::mutex::scoped_lock locker_incoming(incoming_mutex_);
    InternalPushData(data);
    return true;
  }
  void InitSocket(StatusPtr status,
                  boost::asio::ip::tcp::socket *socket);
  const string name() const {
    return name_;
  }
  virtual ~RawConnection();
 protected:
  static const char kHeartBeat = 0xb;
  static const int kDefaultTimeoutMs = 30000;
  static const int kRecvDelayFactor = 2;
  template <class T> void InternalPushData(const T &data);

  SharedConstBuffers *incoming() {
    return &duplex_[incoming_index_];
  }
  SharedConstBuffers *outcoming() {
    return &duplex_[1 - incoming_index_];
  }
  void SwitchIO() {
//    CHECK(outcoming()->empty());
    // Switch the working vector.
    incoming_index_ = 1 - incoming_index_;
  }
  inline void OOBRecv(StatusPtr status, const boost::system::error_code &e, size_t n);
  inline void OOBSend(StatusPtr status, const boost::system::error_code &e);
  inline void HandleRead(StatusPtr status, const boost::system::error_code& e, size_t bytes_transferred);
  inline void HandleWrite(StatusPtr status, const boost::system::error_code& e, size_t byte_transferred);
  virtual bool Decode(size_t byte_transferred) = 0;
  void StartOOBRecv(StatusPtr status);
  void Heartbeat(StatusPtr status);
  scoped_ptr<boost::asio::ip::tcp::socket> socket_;
  string name_;

  char heartbeat_;

  static const int kBufferSize = 8192;
  static const int kHeartbeatUnsyncWindow = 2;
  typedef boost::array<char, kBufferSize> Buffer;
  Buffer buffer_;

  int incoming_index_;
  SharedConstBuffers duplex_[2];
  boost::mutex incoming_mutex_;
  boost::shared_ptr<Connection> connection_;

  int send_package_;
  int recv_package_;
  friend class Connection;
};
// Represents a protocol implementation.
template <typename Decoder>
class RawConnectionImpl : public RawConnection {
public:
  RawConnectionImpl(const string &name,
                boost::shared_ptr<Connection> connection)
    : RawConnection(name, connection) {
  }
protected:
  inline bool Decode(size_t bytes_transferred);
  virtual bool Handle(const Decoder *decoder) = 0;
  Decoder decoder_;
};

template <typename Decoder>
bool RawConnectionImpl<Decoder>::Decode(size_t bytes_transferred) {
  boost::tribool result;
  const char *start = buffer_.data();
  const char *end = start + bytes_transferred;
  const char *p = start;
  while (p < end) {
    boost::tie(result, p) =
      decoder_.Decode(p, end);
    if (result) {
      VLOG(2) << name() << " : " << "Handle lineformat: size: " << (p - start);
      if (!Handle(&decoder_)) {
        return false;
      }
      decoder_.reset();
    } else if (!result) {
      VLOG(2) << name() << " : " << "Parse error";
      return false;
    } else {
      VLOG(2) << name() << " : " << "Need to read more data";
      return true;
    }
  }
  return true;
}
#endif  // RAW_CONNECTION_HPP_
