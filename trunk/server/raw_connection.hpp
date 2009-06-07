// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef RAW_CONNECTION_HPP_
#define RAW_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/shared_const_buffers.hpp"
#include "boost/signals2/signal.hpp"
#include "boost/thread/shared_mutex.hpp"
#include "boost/function.hpp"
#include "boost/smart_ptr.hpp"
#include "server/timer.hpp"
#include "base/atomic.hpp"
class RawConnectionStatus {
 public:
  typedef boost::shared_lock<boost::shared_mutex> Locker;
  RawConnectionStatus() : status_(0), intrusive_count_(0),
     mutex_(new boost::shared_mutex) {
  }
  ~RawConnectionStatus() {
    CHECK_EQ(intrusive_count_, 0);
    if (mutex_) {
      delete mutex_;
      mutex_ = NULL;
      status_ = 0;
    }
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

  int status() const {
    return status_;
  }
  boost::shared_mutex *mutex() {
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
  boost::shared_mutex *mutex_;
};

class Connection;
class RawConnection : public boost::noncopyable {
 public:
  typedef boost::signals2::signal<void()> CloseSignal;
  typedef boost::function1<void, CloseSignal*> CloseSignalRegister;
  RawConnection(const string &name,
                boost::shared_ptr<Connection> connection,
                int timeout);
  void Disconnect();
  bool RegisterCloseListener(boost::function0<void> f);
  bool RegisterCloseSignalByCallback(
      CloseSignalRegister callback);
  bool IsConnected();
  bool ScheduleWrite();
  // The push will take the ownership of the data
  template <typename T>
  inline bool PushData(const T &data) {
    if (status_->mutex() == NULL) {
      return false;
    }
    RawConnectionStatus::Locker locker(*status_->mutex());
    if (status_->closing()) {
      return false;
    }
    boost::mutex::scoped_lock locker_incoming(incoming_mutex_);
    InternalPushData(data);
    return true;
  }
  void InitSocket(boost::asio::ip::tcp::socket *socket);
  const string name() {
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
//    boost::mutex::scoped_lock locker(incoming_mutex_);
    CHECK(outcoming()->empty());
    // Switch the working vector.
    incoming_index_ = 1 - incoming_index_;
  }
  inline void OOBRecv(const boost::system::error_code &e, size_t n);
  inline void OOBSend(const boost::system::error_code &e);
  inline void Timeout(const boost::system::error_code &e);
  inline void OOBWait();
  inline void HandleRead(const boost::system::error_code& e, size_t bytes_transferred);
  inline void HandleWrite(const boost::system::error_code& e, size_t byte_transferred);
  virtual bool Decode(size_t byte_transferred) = 0;
  void InternalStart();
  void InternalDestroy(boost::intrusive_ptr<RawConnectionStatus> status,
                       RawConnection *connection);
   void InternalDestroy();

  scoped_ptr<boost::asio::ip::tcp::socket> socket_;
  string name_;
  boost::shared_ptr<boost::signals2::signal<void()> > close_signal_;
  boost::intrusive_ptr<RawConnectionStatus> status_;
  boost::intrusive_ptr<Timer> send_timer_;
  boost::intrusive_ptr<Timer> recv_timer_;

  int timeout_;
  char heartbeat_;

  static const int kBufferSize = 8192;
  typedef boost::array<char, kBufferSize> Buffer;
  Buffer buffer_;

  int incoming_index_;
  SharedConstBuffers duplex_[2];
  boost::mutex incoming_mutex_;
  boost::shared_ptr<Connection> connection_;
};
// Represents a protocol implementation.
template <typename Decoder>
class RawConnectionImpl : public RawConnection {
public:
  RawConnectionImpl(const string &name,
                boost::shared_ptr<Connection> connection,
                int timeout)
    : RawConnection(name, connection, timeout) {
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
