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
class Connection;
class ConnectionStatus {
 public:
  ConnectionStatus() : status_(0), out_standing_(0) {
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

  void inc_out_standing() {
    ++out_standing_;
  }

  void dec_out_standing() {
    --out_standing_;
  }

  int out_standing() const {
    return out_standing_;
  }

  bool cleanup_posted() {
    return status_ & CLEANUP_POSTED;
  }

  void set_cleanup_posted() {
    status_ |= CLEANUP_POSTED;
  }

  bool time_to_destroy() {
    return out_standing_ == 0 && closing() && !cleanup_posted();
  }
 private:
  enum InternalConnectionStatus {
    IDLE = 0x0,
    READING = 0x01,
    WRITTING = 0x01 << 1,
    CLOSING = 0x01 << 2,
    CLEANUP_POSTED = 0x01 << 3,
  };
  int status_;
  int out_standing_;
};

class Connection {
 public:
  Connection() : timeout_(kDefaultTimeoutMs) {
  }
  inline void Close();
  virtual void set_socket(boost::asio::ip::tcp::socket *socket) {
    socket_.reset(socket);
    boost::asio::socket_base::keep_alive keep_alive(true);
    socket_->set_option(keep_alive);
    boost::asio::socket_base::linger linger(false, 0);
    socket_->set_option(linger);
    // Put the socket into non-blocking mode.
    boost::asio::ip::tcp::socket::non_blocking_io non_blocking_io(true);
    socket_->io_control(non_blocking_io);
    strand_.reset(new boost::asio::io_service::strand(socket_->get_io_service()));
    CHECK_EQ(status_.out_standing(), 0);
    send_timer_.reset(new boost::asio::deadline_timer(socket->get_io_service()));
    recv_timer_.reset(new boost::asio::deadline_timer(socket->get_io_service()));
    status_.inc_out_standing();
    strand_->post(boost::bind(&Connection::OOBSend, this, boost::system::error_code()));
    status_.inc_out_standing();
    strand_->post(boost::bind(&Connection::OOBRecv, this, boost::system::error_code()));
    strand_->post(boost::bind(&Connection::OOBWait, this, false));
  }

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
  virtual void ScheduleRead() = 0;
  virtual void ScheduleWrite() = 0;
  virtual ~Connection() {
    VLOG(2) << name_ << " Distroy connection.";
  }
 protected:
  static const char kHeartBeat = 0xb;
  static const int kDefaultTimeoutMs = 3000;
  static const int kRecvDelayFactor = 2;
  inline void OOBRecv(const boost::system::error_code &e);
  inline void OOBSend(const boost::system::error_code &e);
  inline void Timeout(const boost::system::error_code &e);
  inline void OOBWait(bool cancel);
  inline virtual void Cleanup();
  inline void InternalClose();
  inline void InternalShutdown();
  inline void InternalDestroyOrNot();
  virtual void HandleRead(const boost::system::error_code& e, size_t bytes_transferred) = 0;
  virtual void HandleWrite(const boost::system::error_code& e, size_t byte_transferred) = 0;
  scoped_ptr<boost::asio::io_service::strand> strand_;
  scoped_ptr<boost::asio::ip::tcp::socket> socket_;
  string name_;
  boost::signals2::signal<void()> close_signal_;
  ConnectionStatus status_;
  boost::scoped_ptr<boost::asio::deadline_timer> send_timer_;
  boost::scoped_ptr<boost::asio::deadline_timer> recv_timer_;
  int timeout_;
  char heartbeat_;
  boost::mutex closing_mutex_;
};

// Represents a single Connection from a client.
// The Handler::Handle method should be multi thread safe.
template <typename Decoder>
class ConnectionImpl : virtual public Connection {
public:
  ConnectionImpl() : Connection(), incoming_index_(0), decoder_(new Decoder) {
  }
  Connection* Clone()  = 0;
  // ScheduleRead the first asynchronous operation for the Connection.
  void ScheduleRead();

  void ScheduleWrite();

  template <typename T>
  // The push will take the ownership of the data
  void PushData(const T &data) {
    boost::mutex::scoped_lock locker(incoming_mutex_);
    return InternalPushData(data);
  }
 protected:
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
  void InternalScheduleRead();
  void InternalScheduleWrite();
  template <class T> void InternalPushData(const T &data);
  virtual void Handle(boost::shared_ptr<const Decoder> decoder) = 0;
  void HandleRead(const boost::system::error_code& e, size_t bytes_transferred);
  virtual void HandleWrite(const boost::system::error_code& e, size_t byte_transferred);
  static const int kBufferSize = 8192;
  typedef boost::array<char, kBufferSize> Buffer;

  Buffer buffer_;

  int incoming_index_;
  boost::mutex incoming_mutex_;
  SharedConstBuffers duplex_[2];
  boost::shared_ptr<Decoder> decoder_;
};

void Connection::Close() {
  boost::mutex::scoped_lock lock(closing_mutex_);
  if (status_.closing()) {
    VLOG(2) << name() << " call Close but is closing";
    return;
  }
  status_.inc_out_standing();
  strand_->post(boost::bind(&Connection::InternalShutdown, this));
}

void Connection::OOBSend(const boost::system::error_code &e) {
  VLOG(2) << name() << " : " << "OOBSend" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  status_.dec_out_standing();
  if (status_.closing() || e) {
    InternalDestroyOrNot();
    return;
  }
  char heartbeat = kHeartBeat;
  int n= socket_->send(boost::asio::buffer(&heartbeat, sizeof(heartbeat)),
                       boost::asio::socket_base::message_out_of_band);
  if (n != sizeof(heartbeat)) {
    VLOG(2) << name() << " : " << "OOBSend error, n:" << n;
    InternalDestroyOrNot();
    return;
  }
  send_timer_->expires_from_now(boost::posix_time::milliseconds(timeout_));
  status_.inc_out_standing();
  send_timer_->async_wait(
      strand_->wrap(boost::bind(&Connection::OOBSend, this, _1)));
}

void Connection::OOBRecv(const boost::system::error_code &e) {
  VLOG(2) << name() << " : " << "OOBRecv" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  status_.dec_out_standing();
  VLOG(2) << name() << " : " << "OOBRecv error: " << e.message();
  if (status_.closing() || e) {
    InternalDestroyOrNot();
    return;
  }
  status_.inc_out_standing();
  socket_->async_receive(boost::asio::buffer(&heartbeat_, sizeof(heartbeat_)),
                         boost::asio::socket_base::message_out_of_band,
                         strand_->wrap(boost::bind(&Connection::OOBRecv, this, _1)));
}

void Connection::Timeout(const boost::system::error_code &e) {
  VLOG(2) << name() << " : " << "Timeout" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  status_.dec_out_standing();
  if (status_.closing() || (e && (e != boost::asio::error::operation_aborted))) {
    LOG(WARNING) << name() << " : " << "Timeouted";
    InternalDestroyOrNot();
    return;
  }
}

void Connection::OOBWait(bool cancel) {
  VLOG(2) << name() << " : " << "OOBWait" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  // cancel.
  if (cancel) {
    recv_timer_->cancel();
  }
  recv_timer_->expires_from_now(boost::posix_time::milliseconds(
      timeout_ * kRecvDelayFactor));
  status_.inc_out_standing();
  recv_timer_->async_wait(
      strand_->wrap(boost::bind(&Connection::Timeout, this, _1)));
  return;
}

void Connection::InternalShutdown() {
  status_.dec_out_standing();
  VLOG(2) << name() << " : " << "InternalShutdown" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  if (status_.closing()) {
    VLOG(2) << "Call InternalShutdown but is closing";
    return;
  }
  boost::system::error_code e;
  socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, e);
  if (e) {
    VLOG(2) << "Shutdown error:" << e;
  }
  InternalDestroyOrNot();
}

void Connection::InternalDestroyOrNot() {
  VLOG(2) << name() << " : " << "InternalDestroyOrNot" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  if (!status_.closing()) {
    boost::mutex::scoped_lock lock(closing_mutex_);
    status_.set_closing();
    strand_->post(boost::bind(&Connection::InternalClose, this));
    return;
  }
  if (status_.time_to_destroy()) {
    status_.set_cleanup_posted();
    strand_->post(boost::bind(&Connection::Cleanup, this));
  }
}

void Connection::InternalClose() {
  VLOG(2) << name() << " : " << "InternalClose" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  socket_->close();
  recv_timer_->cancel();
  send_timer_->cancel();
  InternalDestroyOrNot();
}

void Connection::Cleanup() {
  VLOG(2) << name() << " : " << "Cleanup" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  VLOG(2) << name() << "Connection::Cleanup, num_slots: " << close_signal_.num_slots();
  close_signal_();
}

template <class Decoder>
void ConnectionImpl<Decoder>::HandleRead(const boost::system::error_code& e,
                                         size_t bytes_transferred) {
  VLOG(2) << name() << " : " << "HandleRead" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  VLOG(2) << name() << " HandleRead, e: " << e.message() << ", bytes: "
    << bytes_transferred;
  CHECK(status_.reading());
  status_.dec_out_standing();
  if (e) {
    status_.clear_reading();
    InternalDestroyOrNot();
    return;
  }

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
  status_.inc_out_standing();
  socket_->async_read_some(
      boost::asio::buffer(buffer_),
      strand_->wrap(boost::bind(&ConnectionImpl<Decoder>::HandleRead, this, _1, _2)));
  OOBWait(true);
  return;
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleRead() {
  boost::mutex::scoped_lock locker(closing_mutex_);
  if (status_.closing()) {
    VLOG(2) << name() << " : " << "ScheduleRead but strand is closing";
    return;
  }
  status_.inc_out_standing();
  strand_->post(boost::bind(&ConnectionImpl<Decoder>::InternalScheduleRead, this));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::InternalScheduleRead() {
  status_.dec_out_standing();
  VLOG(2) << name() << " : " << "InternalScheduleRead" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  if (status_.closing()) {
    InternalDestroyOrNot();
    VLOG(2) << name() << " : " << "InternalScheduleRead but closing";
    return;
  }
  if (status_.reading()) {
    VLOG(2) << name() << " : " << "InternalScheduleRead but already reading";
    return;
  }
  status_.set_reading();
  status_.inc_out_standing();
  socket_->async_read_some(
      boost::asio::buffer(buffer_),
      strand_->wrap(boost::bind(&ConnectionImpl<Decoder>::HandleRead, this, _1, _2)));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleWrite() {
  boost::mutex::scoped_lock locker(closing_mutex_);
  if (status_.closing()) {
    VLOG(2) << name() << " : " << "ScheduleWrite but is closing";
    return;
  }
  status_.inc_out_standing();
  strand_->post(boost::bind(&ConnectionImpl<Decoder>::InternalScheduleWrite, this));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::InternalScheduleWrite() {
  status_.dec_out_standing();
  VLOG(2) << name() << " : " << "InternalScheduleWrite" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  if (status_.closing()) {
    InternalDestroyOrNot();
    VLOG(2) << name() << " : " << " InternalScheduleWrite but closing";
    return;
  }
  if (status_.writting()) {
    VLOG(2) << name() << " : " << " InternalScheduleWrite but already writting";
    return;
  }
  status_.set_writting();
  VLOG(2) << name() << " duplex_[0] : " << duplex_[0].empty();
  VLOG(2) << name() << " duplex_[1] : " << duplex_[1].empty();
  VLOG(2) << name() << " incoming: " << incoming_index_;
  SwitchIO();
  if (outcoming()->empty()) {
    SwitchIO();
    if (outcoming()->empty()) {
      status_.clear_writting();
      VLOG(2) << name() << " : " << "No outcoming";
      return;
    }
  }

  status_.inc_out_standing();
  socket_->async_write_some(
      *outcoming(),
      strand_->wrap(boost::bind(&Connection::HandleWrite, this, _1, _2)));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::HandleWrite(const boost::system::error_code& e, size_t byte_transferred) {
  VLOG(2) << name() << " : " << "HandleWrite" << " status: " << status_.status() << " out_standing: " << status_.out_standing();
  VLOG(2) << name() << " : " << "HandleWrite bytes: " << byte_transferred << " status: " << status_.status();
  CHECK(status_.writting());
  status_.dec_out_standing();
  if (e) {
    status_.clear_writting();
    InternalDestroyOrNot();
    return;
  }
  outcoming()->consume(byte_transferred);
  if (!outcoming()->empty()) {
    VLOG(2) << name() << " outcoming is not empty, size" << outcoming()->size();
    status_.inc_out_standing();
    socket_->async_write_some(
        *outcoming(),
        strand_->wrap(boost::bind(&ConnectionImpl<Decoder>::HandleWrite, this, _1, _2)));
  } else {
    VLOG(2) << name() << " : " << "outcoming is empty";
    outcoming()->clear();
    SwitchIO();
    if (!outcoming()->empty()) {
      VLOG(2) << name() << " : " << "outcoming is not empty after SwitchIO, size" << outcoming()->size();
      status_.inc_out_standing();
      socket_->async_write_some(
          *outcoming(),
          strand_->wrap(boost::bind(&ConnectionImpl<Decoder>::HandleWrite, this, _1, _2)));
    } else {
      status_.clear_writting();
    }
  }
}
#endif // NET2_CONNECTION_HPP_
