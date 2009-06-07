// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#define RawConnTrace VLOG(2) << name() << " : " << __func__ << " status: " << status_->status() << " "

#include "server/connection.hpp"
#include "server/raw_connection.hpp"
#include "base/base.hpp"
#include "base/executor.hpp"
#include "glog/logging.h"
#include "server/meta.pb.h"
#include "protobuf/service.h"
#include "boost/thread.hpp"
#include "server/shared_const_buffers.hpp"
#include "boost/signals2/signal.hpp"
class ExecuteHandler {
 public:
  ExecuteHandler(boost::intrusive_ptr<RawConnectionStatus> status,
               RawConnection *connection,
               void (RawConnection::*member)())
    : status_(status), connection_(connection), member_(member) {
  }
  void operator()() {
    if (status_->closing()) {
      return;
    }
    boost::shared_mutex *mut = status_->mutex();
    if (mut == NULL) {
      return;
    }
    RawConnectionStatus::Locker locker(*mut);
    if (!status_->closing()) {
      (connection_->*member_)();
    }
  }
 private:
  boost::intrusive_ptr<RawConnectionStatus> status_;
  RawConnection *connection_;
  void (RawConnection::*member_)();
};

class WaitHandler {
 public:
  WaitHandler(boost::intrusive_ptr<RawConnectionStatus> status,
              boost::intrusive_ptr<Timer> timer,
            RawConnection *connection,
            void (RawConnection::*member)(const boost::system::error_code&))
    : status_(status), timer_(timer),
      connection_(connection), member_(member) {
  }
  void operator()(const boost::system::error_code& e) {
    if (status_->closing()) {
      return;
    }
    boost::shared_mutex *mut = status_->mutex();
    if (mut == NULL) {
      return;
    }
    RawConnectionStatus::Locker locker(*mut);
    if (!status_->closing()) {
      (connection_->*member_)(e);
    }
  }
 private:
  boost::intrusive_ptr<RawConnectionStatus> status_;
  boost::intrusive_ptr<Timer> timer_;
  RawConnection *connection_;
  void (RawConnection::*member_)(const boost::system::error_code&);
};

class ReadHandler {
 public:
  ReadHandler(boost::intrusive_ptr<RawConnectionStatus> status,
              RawConnection *connection,
              void (RawConnection::*member)(
                  const boost::system::error_code &, size_t))
    : status_(status), connection_(connection), member_(member) {
  }
  void operator()(const boost::system::error_code &e, size_t n) {
    if (status_->closing()) {
      return;
    }
    boost::shared_mutex *mut = status_->mutex();
    if (mut == NULL) {
      return;
    }
    RawConnectionStatus::Locker locker(*mut);
    if (!status_->closing()) {
      (connection_->*member_)(e, n);
    }
  }
 private:
  boost::intrusive_ptr<RawConnectionStatus> status_;
  RawConnection *connection_;
  void (RawConnection::*member_)(const boost::system::error_code&, size_t);
};
typedef ReadHandler WriteHandler;

RawConnection::RawConnection(const string &name,
                             boost::shared_ptr<Connection> connection,
                             int timeout)
  : name_(name),
    timeout_(timeout),
    incoming_index_(0),
    status_(new RawConnectionStatus),
    connection_(connection),
    close_signal_(new boost::signals2::signal<void()>) {
}

void RawConnection::InitSocket(boost::asio::ip::tcp::socket *socket) {
  RawConnectionStatus::Locker locker(*status_->mutex());
  CHECK(!status_->closing());
  RawConnTrace;
  socket_.reset(socket);
  boost::asio::socket_base::keep_alive keep_alive(true);
  socket_->set_option(keep_alive);
  boost::asio::socket_base::linger linger(false, 0);
  socket_->set_option(linger);
  // Put the socket into non-blocking mode.
  boost::asio::ip::tcp::socket::non_blocking_io non_blocking_io(true);
  socket_->io_control(non_blocking_io);
  send_timer_.reset(new Timer(socket->get_io_service()));
  recv_timer_.reset(new Timer(socket->get_io_service()));
  ExecuteHandler handler(status_, this, &RawConnection::InternalStart);
  socket_->get_io_service().post(handler);
  status_->set_reading();
  ReadHandler h(status_, this, &RawConnection::HandleRead);
  socket_->async_read_some(
      boost::asio::buffer(buffer_), h);
}

void RawConnection::Disconnect() {
  boost::shared_mutex *mut = status_->mutex();
  if (mut == NULL) {
    return;
  }
  mut->lock_upgrade();
  mut->unlock_upgrade_and_lock();
  if (status_->closing()) {
    RawConnTrace << "Already closing";
    mut->unlock();
    return;
  }
  status_->set_closing();
  mut->unlock();
  if (close_signal_.get()) {
    close_signal_->disconnect_all_slots();
  }
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
  delete this;
}

RawConnection::~RawConnection() {
  VLOG(2) << name() << "~RawConnection";
  CHECK(status_->closing());
}

void RawConnection::InternalStart() {
  OOBSend(boost::system::error_code());
  OOBRecv(boost::system::error_code(), 0);
}

bool RawConnection::IsConnected() {
  if (status_->mutex() == NULL) {
    return false;
  }
  RawConnectionStatus::Locker locker(*status_->mutex());
  return (!status_->closing()) &&status_->reading() && socket_ && socket_->is_open();
}

bool RawConnection::RegisterCloseListener(boost::function0<void> f) {
  if (status_->mutex() == NULL) {
    return false;
  }
  RawConnectionStatus::Locker locker(*status_->mutex());
  if (status_->closing()) {
    RawConnTrace << "is closing";
    return false;
  }
  if (close_signal_.get() == NULL) {
    return false;
  }
  close_signal_->connect(f);
  return true;
}

bool RawConnection::RegisterCloseSignalByCallback(
      CloseSignalRegister callback) {
  if (status_->mutex() == NULL) {
    return false;
  }
  RawConnectionStatus::Locker locker(*status_->mutex());
  if (status_->closing()) {
    RawConnTrace << "is closing";
    return false;
  }
  if (close_signal_.get() == NULL) {
    return false;
  }
  callback(close_signal_.get());
  return true;
}

void RawConnection::InternalDestroy(boost::intrusive_ptr<RawConnectionStatus> status,
                                    RawConnection *connection) {
  {
    boost::shared_mutex *mut = status->mutex();
    if (mut == NULL) {
      return;
    }
    mut->unlock_shared();
    mut->lock_upgrade();
    mut->unlock_upgrade_and_lock();
    if (status->closing()) {
      // RawConnTrace << "Already closing";
      mut->unlock_and_lock_shared();
      return;
    }
    status->set_closing();
    mut->unlock_and_lock_shared();
  }
  connection->connection_->ImplClosed();
  if (connection->recv_timer_.get()) {
    connection->recv_timer_->cancel();
    connection->recv_timer_.reset();
  }
  if (connection->send_timer_.get()) {
    connection->send_timer_->cancel();
    connection->send_timer_.reset();
  }
  if (connection->socket_.get()) {
    connection->socket_->close();
    connection->socket_.reset();
  }
  (*connection->close_signal_)();
  connection->close_signal_->disconnect_all_slots();
  delete connection;
}

void RawConnection::OOBSend(const boost::system::error_code &e) {
  if (e != boost::asio::error::operation_aborted) {
    char heartbeat = kHeartBeat;
    boost::system::error_code ec;
    int n= socket_->send(boost::asio::buffer(&heartbeat, sizeof(heartbeat)),
                         boost::asio::socket_base::message_out_of_band, ec);
    if (ec || (n != sizeof(heartbeat))) {
      VLOG(2) << name() << " : " << "OOBSend error, n:" << n << " ec: " << ec.message();
      InternalDestroy(status_, this);
      return;
    }
    send_timer_->expires_from_now(boost::posix_time::milliseconds(timeout_));
    WaitHandler h(status_, send_timer_, this, &RawConnection::OOBSend);
    send_timer_->async_wait(h);
  }
}

void RawConnection::OOBRecv(const boost::system::error_code &e, size_t n) {
  if (e) {
    InternalDestroy(status_, this);
    return;
  }

  OOBWait();

  ReadHandler h(status_, this, &RawConnection::OOBRecv);
  socket_->async_receive(boost::asio::buffer(&heartbeat_, sizeof(heartbeat_)),
                         boost::asio::socket_base::message_out_of_band,
                         h);
}

void RawConnection::Timeout(const boost::system::error_code &e) {
  if (e != boost::asio::error::operation_aborted) {
    LOG(WARNING) << name() << " : " << "Timeouted";
    InternalDestroy(status_, this);
  }
}

void RawConnection::OOBWait() {
  recv_timer_->expires_from_now(boost::posix_time::milliseconds(
      timeout_ * kRecvDelayFactor));
  WaitHandler h(status_, recv_timer_, this, &RawConnection::Timeout);
  recv_timer_->async_wait(h);
}

void RawConnection::HandleRead(const boost::system::error_code& e,
                               size_t bytes_transferred) {
  CHECK(status_->reading());
  RawConnTrace<< " e:" << e.message() << " bytes: " << bytes_transferred;
  if (e) {
    status_->clear_reading();
    InternalDestroy(status_, this);
    return;
  }
  if (!Decode(bytes_transferred)) {
    RawConnTrace << "Decoder error";
    status_->clear_reading();
    InternalDestroy(status_, this);
    return;
  }
  ReadHandler h(status_, this, &RawConnection::HandleRead);
  socket_->async_read_some(
      boost::asio::buffer(buffer_),
      h);
  OOBWait();
  return;
}

bool RawConnection::ScheduleWrite() {
  if (status_->mutex() == NULL) {
    return false;
  }
  RawConnectionStatus::Locker locker(*status_->mutex());
  if (status_->closing()) {
    RawConnTrace << "ScheduleWrite but is closing";
    return false;
  }
  RawConnTrace;
  boost::mutex::scoped_lock incoming_locker(incoming_mutex_);
  if (status_->writting()) {
    RawConnTrace << " : " << "ScheduleWrite but already writting";
    return true;
  }
  status_->set_writting();
  RawConnTrace << "duplex_[0] : " << duplex_[0].empty();
  RawConnTrace << "duplex_[1] : " << duplex_[1].empty();
  RawConnTrace << "incoming: " << incoming_index_;
  SwitchIO();
  if (outcoming()->empty()) {
    status_->clear_writting();
    RawConnTrace << "No outcoming";
    return true;
  }

  WriteHandler h(status_, this, &RawConnection::HandleWrite);
  socket_->async_write_some(
      *outcoming(),
      h);
  return true;
}

void RawConnection::HandleWrite(const boost::system::error_code& e, size_t byte_transferred) {
  CHECK(status_->writting());
  RawConnTrace << "e:" << e.message() << " byte_transferred: " << byte_transferred;
  if (e) {
    status_->clear_writting();
    InternalDestroy(status_, this);
    return;
  }
  boost::mutex::scoped_lock locker(incoming_mutex_);
  outcoming()->consume(byte_transferred);
  if (!outcoming()->empty()) {
    WriteHandler h(status_, this, &RawConnection::HandleWrite);
    socket_->async_write_some(
        *outcoming(),
        h);
  } else {
    RawConnTrace << "outcoming is empty";
    SwitchIO();
    if (!outcoming()->empty()) {
      RawConnTrace << " : " << "outcoming is not empty after SwitchIO, size :" << outcoming()->size();
      WriteHandler h(status_, this, &RawConnection::HandleWrite);
      socket_->async_write_some(
          *outcoming(),
          h);
    } else {
      status_->clear_writting();
    }
  }
}
#undef RawConnTrace
