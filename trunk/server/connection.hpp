// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef NET2_CONNECTION_HPP_
#define NET2_CONNECTION_HPP_
#include "base/base.hpp"
#include "base/executor.hpp"
#include "glog/logging.h"
#include "server/meta.pb.h"
#include "protobuf/service.h"
#include "boost/thread.hpp"
#include "server/shared_const_buffers.hpp"
#include "boost/signals2/signal.hpp"
#include "server/timer.hpp"
#define ConnTrace VLOG(2) << name() << " : " << __func__ << " status: " << status_->status() << " "
class Connection;
class ConnectionStatus {
 public:
  ConnectionStatus() : status_(0) {
  }
  bool reading() const {
    return status_ & READING;
  }

  bool writting() const {
    return status_ & WRITTING;
  }

  void set_reading() {
    status_ |= READING;
  }

  void set_writting() {
    status_ |= WRITTING;
  }

  void clear_reading() {
    status_ &= ~READING;
  }

  void clear_writting() {
    status_ &= ~WRITTING;
  }

  void set_closing() {
    status_ |= CLOSING;
  }

  bool closing() const {
    return status_ & CLOSING;
  }

  int status() const {
    return status_;
  }
 private:
  enum InternalConnectionStatus {
    IDLE = 0x0,
    READING = 0x01,
    WRITTING = 0x01 << 1,
    CLOSING = 0x01 << 2,
  };
  int status_;
};

class Connection {
 public:
  Connection() : timeout_(kDefaultTimeoutMs), incoming_index_(0), status_(new ConnectionStatus), mutex_(new boost::shared_mutex) {
  }
  inline void Close();

  inline void set_socket(boost::asio::ip::tcp::socket *socket);

  void set_timeout(int timeout_ms) {
    timeout_ = timeout_ms;
  }

  void set_name(const string name) {
    name_ = name;
  }

  const string name() const {
    return name_;
  }

  boost::signals2::signal<void()> *close_signal() {
    return &close_signal_;
  }

  virtual bool IsConnected() const {
    return socket_ && socket_->is_open();
  }
  virtual Connection* Clone() = 0;
  inline void ScheduleRead();
  inline void ScheduleWrite();
  template <typename T>
  // The push will take the ownership of the data
  inline void PushData(const T &data) {
    boost::mutex::scoped_lock locker(incoming_mutex_);
    return InternalPushData(data);
  }
  virtual ~Connection() {
    CHECK(!IsConnected());
  }
 protected:
  static const char kHeartBeat = 0xb;
  static const int kDefaultTimeoutMs = 30000;
  static const int kRecvDelayFactor = 2;

  template <class T> void InternalPushData(const T &data);

  void InternalClose() {
    Destroy(true);
  }

  SharedConstBuffers *incoming() {
    return &duplex_[incoming_index_];
  }
  SharedConstBuffers *outcoming() {
    return &duplex_[1 - incoming_index_];
  }
  void SwitchIO() {
    boost::mutex::scoped_lock locker(incoming_mutex_);
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
  inline void InternalScheduleRead();
  inline void InternalScheduleWrite();
  inline void Destroy(bool destroy);
  virtual void Decode(size_t byte_transferred) = 0;

  scoped_ptr<boost::asio::ip::tcp::socket> socket_;
  scoped_ptr<boost::asio::io_service::strand> strand_;
  string name_;
  boost::signals2::signal<void()> close_signal_;
  boost::shared_ptr<ConnectionStatus> status_;
  boost::scoped_ptr<boost::asio::deadline_timer> send_timer_;
  boost::scoped_ptr<boost::asio::deadline_timer> recv_timer_;

  int timeout_;
  char heartbeat_;

  boost::shared_ptr<boost::shared_mutex> mutex_;

  static const int kBufferSize = 8192;
  typedef boost::array<char, kBufferSize> Buffer;
  Buffer buffer_;

  int incoming_index_;
  SharedConstBuffers duplex_[2];
  boost::mutex incoming_mutex_;
};

class ExecuteHandler {
 public:
  ExecuteHandler(boost::shared_ptr<ConnectionStatus> status,
               boost::shared_ptr<boost::shared_mutex> mutex,
               Connection *connection,
               void (Connection::*member)())
    : status_(status), mutex_(mutex), connection_(connection), member_(member) {
  }
  void operator()() {
    mutex_->lock_shared();
    if (!status_->closing()) {
      (connection_->*member_)();
    }
    mutex_->unlock_shared();
  }
 private:
  boost::shared_ptr<ConnectionStatus> status_;
  boost::shared_ptr<boost::shared_mutex> mutex_;
  Connection *connection_;
  void (Connection::*member_)();
};

class WaitHandler {
 public:
  WaitHandler(boost::shared_ptr<ConnectionStatus> status,
            boost::shared_ptr<boost::shared_mutex> mutex,
            Connection *connection,
            void (Connection::*member)(const boost::system::error_code&))
    : status_(status), mutex_(mutex), connection_(connection), member_(member) {
  }
  void operator()(const boost::system::error_code& e) {
    mutex_->lock_shared();
    if (!status_->closing()) {
      (connection_->*member_)(e);
    }
    mutex_->unlock_shared();
  }
 private:
  boost::shared_ptr<ConnectionStatus> status_;
  boost::shared_ptr<boost::shared_mutex> mutex_;
  Connection *connection_;
  void (Connection::*member_)(const boost::system::error_code&);
};

class ReadHandler {
 public:
  ReadHandler(boost::shared_ptr<ConnectionStatus> status,
               boost::shared_ptr<boost::shared_mutex> mutex,
               Connection *connection,
               void (Connection::*member)(const boost::system::error_code &, size_t))
    : status_(status), mutex_(mutex), connection_(connection), member_(member) {
  }
  void operator()(const boost::system::error_code &e, size_t n) {
    mutex_->lock_shared();
    if (!status_->closing()) {
      (connection_->*member_)(e, n);
    }
    mutex_->unlock_shared();
  }
 private:
  boost::shared_ptr<ConnectionStatus> status_;
  boost::shared_ptr<boost::shared_mutex> mutex_;
  Connection *connection_;
  void (Connection::*member_)(const boost::system::error_code&, size_t);
};
typedef ReadHandler WriteHandler;

// Represents a single Connection from a client.
// The Handler::Handle method should be multi thread safe.
template <typename Decoder>
class ConnectionImpl : virtual public Connection {
public:
  ConnectionImpl() : Connection(),  decoder_(new Decoder) {
  }
protected:
  void Decode(size_t bytes_transferred);
  virtual void Handle(boost::shared_ptr<const Decoder> decoder) = 0;
  boost::shared_ptr<Decoder> decoder_;
};

void Connection::set_socket(boost::asio::ip::tcp::socket *socket) {
  mutex_->lock();
  if (socket_.get()) {
    ConnTrace << "Already set socket";
    mutex_->unlock();
    return;
  }
  ConnTrace;
  socket_.reset(socket);
  boost::asio::socket_base::keep_alive keep_alive(true);
  socket_->set_option(keep_alive);
  boost::asio::socket_base::linger linger(false, 0);
  socket_->set_option(linger);
  // Put the socket into non-blocking mode.
  boost::asio::ip::tcp::socket::non_blocking_io non_blocking_io(true);
  socket_->io_control(non_blocking_io);
  strand_.reset(new boost::asio::io_service::strand(socket_->get_io_service()));
  send_timer_.reset(new boost::asio::deadline_timer(socket->get_io_service()));
  recv_timer_.reset(new boost::asio::deadline_timer(socket->get_io_service()));
  OOBSend(boost::system::error_code());
  OOBRecv(boost::system::error_code(), 0);
  mutex_->unlock();
}

void Connection::Close() {
  mutex_->lock();
  if (!IsConnected()) {
    ConnTrace << "Already closed";
    return;
  }
  Destroy(true);
//  strand_->post(ExecuteHandler(status_, mutex_, this, &Connection::InternalDestroy));
  mutex_->unlock();
}

void Connection::Destroy(bool destroy) {
  {
    if (status_->closing()) {
      ConnTrace << "Already closing";
      return;
    }
    status_->set_closing();
    if (recv_timer_.get()) {
      recv_timer_->cancel();
      recv_timer_.reset();
    }
    if (send_timer_.get()) {
      send_timer_->cancel();
      send_timer_.reset();
    }
    if (socket_.get()) {
      socket_->close();
      socket_.reset();
    }
  }
  close_signal_();
  close_signal_.disconnect_all_slots();
  if (destroy) {
    delete this;
  }
}

void Connection::OOBSend(const boost::system::error_code &e) {
  char heartbeat = kHeartBeat;
  int n= socket_->send(boost::asio::buffer(&heartbeat, sizeof(heartbeat)),
                       boost::asio::socket_base::message_out_of_band);
  if (n != sizeof(heartbeat)) {
    VLOG(2) << name() << " : " << "OOBSend error, n:" << n;
    Destroy(true);
    return;
  }
  send_timer_->expires_from_now(boost::posix_time::milliseconds(timeout_));
  send_timer_->async_wait(strand_->wrap(WaitHandler(status_, mutex_, this, &Connection::OOBSend)));
}

void Connection::OOBRecv(const boost::system::error_code &e, size_t n) {
  if (e) {
    Destroy(true);
    return;
  }

  OOBWait();

  socket_->async_receive(boost::asio::buffer(&heartbeat_, sizeof(heartbeat_)),
                         boost::asio::socket_base::message_out_of_band,
                         strand_->wrap(ReadHandler::ReadHandler(status_, mutex_, this, &Connection::OOBRecv)));
}

void Connection::Timeout(const boost::system::error_code &e) {
  if (e != boost::asio::error::operation_aborted) {
    LOG(WARNING) << name() << " : " << "Timeouted";
    Destroy(true);
  }
}

void Connection::OOBWait() {
  recv_timer_->expires_from_now(boost::posix_time::milliseconds(
      timeout_ * kRecvDelayFactor));
  recv_timer_->async_wait(strand_->wrap(WaitHandler(status_, mutex_, this, &Connection::Timeout)));
}

void Connection::HandleRead(const boost::system::error_code& e,
                            size_t bytes_transferred) {
  CHECK(status_->reading());
  ConnTrace<< " e:" << e.message() << " bytes: " << bytes_transferred;
  if (e) {
    Destroy(true);
    return;
  }
  Decode(bytes_transferred);
  socket_->async_read_some(
      boost::asio::buffer(buffer_),
      strand_->wrap(ReadHandler(status_, mutex_, this, &Connection::HandleRead)));
  OOBWait();
  return;
}

void Connection::ScheduleRead() {
  mutex_->lock_shared();
  if (status_->reading()) {
    ConnTrace << "ScheduleRead but already reading";
    mutex_->unlock_shared();
    return;
  }
  status_->set_reading();
  socket_->async_read_some(
      boost::asio::buffer(buffer_),
      strand_->wrap(ReadHandler(status_, mutex_, this, &Connection::HandleRead)));
  mutex_->unlock_shared();
}

void Connection::ScheduleWrite() {
  mutex_->lock_shared();
  ConnTrace;
  if (status_->writting()) {
    ConnTrace << " : " << "ScheduleWrite but already writting";
    mutex_->unlock_shared();
    return;
  }
  status_->set_writting();
  ConnTrace << "duplex_[0] : " << duplex_[0].empty();
  ConnTrace << "duplex_[1] : " << duplex_[1].empty();
  ConnTrace << "incoming: " << incoming_index_;
  SwitchIO();
  if (outcoming()->empty()) {
    SwitchIO();
    if (outcoming()->empty()) {
      status_->clear_writting();
      ConnTrace << "No outcoming";
      mutex_->unlock_shared();
      return;
    }
  }

  socket_->async_write_some(
      *outcoming(),
      strand_->wrap(WriteHandler(status_, mutex_, this, &Connection::HandleWrite)));
  mutex_->unlock_shared();
}

void Connection::HandleWrite(const boost::system::error_code& e, size_t byte_transferred) {
  CHECK(status_->writting());
  if (e) {
    Destroy(true);
    return;
  }
  outcoming()->consume(byte_transferred);
  if (!outcoming()->empty()) {
    socket_->async_write_some(
        *outcoming(),
        strand_->wrap(WriteHandler(status_, mutex_, this, &Connection::HandleWrite)));
  } else {
    ConnTrace << "outcoming is empty";
    outcoming()->clear();
    SwitchIO();
    if (!outcoming()->empty()) {
      ConnTrace << " : " << "outcoming is not empty after SwitchIO, size :" << outcoming()->size();
      socket_->async_write_some(
          *outcoming(),
          strand_->wrap(WriteHandler(status_, mutex_, this, &Connection::HandleWrite)));
    } else {
      status_->clear_writting();
    }
  }
}

template <typename Decoder>
void ConnectionImpl<Decoder>::Decode(size_t bytes_transferred) {
  boost::tribool result;
  const char *start = buffer_.data();
  const char *end = start + bytes_transferred;
  const char *p = start;
  while (p < end) {
    boost::tie(result, p) =
      decoder_->Decode(p, end);
    if (result) {
      VLOG(2) << name() << " : " << "Handle lineformat: size: " << (p - start);
      boost::shared_ptr<const Decoder> shared_decoder(decoder_);
      decoder_.reset(new Decoder);
      Handle(shared_decoder);
    } else if (!result) {
      VLOG(2) << name() << " : " << "Parse error";
      break;
    } else {
      VLOG(2) << name() << " : " << "Need to read more data";
      break;
    }
  }
}
#undef ConnTrace
#endif // NET2_CONNECTION_HPP_
