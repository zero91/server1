// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#define RawConnTrace VLOG(2) << name() << " : " << __func__ << " status: " << status->status() << " "

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
  ExecuteHandler(const RawConnection::StatusPtr &status,
                 RawConnection *connection,
                 void (RawConnection::*member)(RawConnection::StatusPtr))
    : status_(status), connection_(connection), member_(member) {
  }
  void operator()() {
    status_->mutex().lock_shared();
    if (status_->closing()) {
      status_->mutex().unlock_shared();
      return;
    }
    (connection_->*member_)(status_);
  }
 private:
  RawConnection::StatusPtr status_;
  RawConnection *connection_;
  void (RawConnection::*member_)(RawConnection::StatusPtr);
};

class WaitHandler {
 public:
  WaitHandler(const RawConnection::StatusPtr &status,
              boost::shared_ptr<Timer> timer,
              RawConnection *connection,
            void (RawConnection::*member)(
                RawConnection::StatusPtr status,
                const boost::system::error_code&))
    : status_(status), connection_(connection), timer_(timer), member_(member) {
  }
  void operator()(const boost::system::error_code& e) {
    if (status_->closing()) {
      return;
    }
    status_->mutex().lock_shared();
    if (status_->closing()) {
      status_->mutex().unlock_shared();
      return;
    }
    (connection_->*member_)(status_, e);
  }
 private:
  RawConnection::StatusPtr status_;
  RawConnection *connection_;
  boost::shared_ptr<Timer> timer_;
  void (RawConnection::*member_)(RawConnection::StatusPtr status,
                                 const boost::system::error_code&);
};

class ReadHandler {
 public:
  ReadHandler(const RawConnection::StatusPtr &status,
              RawConnection *connection,
              void (RawConnection::*member)(
                  RawConnection::StatusPtr status,
                  const boost::system::error_code &, size_t))
    : status_(status), connection_(connection), member_(member) {
  }
  void operator()(const boost::system::error_code &e, size_t n) {
    status_->mutex().lock_shared();
    if (status_->closing()) {
      status_->mutex().unlock_shared();
      return;
    }
    (connection_->*member_)(status_, e, n);
  }
 private:
  RawConnection::StatusPtr status_;
  RawConnection *connection_;
  void (RawConnection::*member_)(
      RawConnection::StatusPtr status, const boost::system::error_code&, size_t);
};
typedef ReadHandler WriteHandler;

RawConnection::RawConnection(const string &name,
                             boost::shared_ptr<Connection> connection)
  : name_(name),
    incoming_index_(0),
    connection_(connection) {
}

void RawConnection::InitSocket(
    StatusPtr status,
    boost::asio::ip::tcp::socket *socket,
    boost::shared_ptr<Timer> timer) {
  RawConnectionStatus::Locker locker(status->mutex());
  CHECK(!status->closing());
  RawConnTrace;
  socket_.reset(socket);
  boost::asio::socket_base::keep_alive keep_alive(true);
  socket_->set_option(keep_alive);
  boost::asio::socket_base::linger linger(false, 0);
  socket_->set_option(linger);
  // Put the socket into non-blocking mode.
  boost::asio::ip::tcp::socket::non_blocking_io non_blocking_io(true);
  socket_->io_control(non_blocking_io);
  send_timer_ = timer;
  StartOOBSend(status);
  StartOOBRecv(status);
  status->set_reading();
  ReadHandler h(status, this, &RawConnection::HandleRead);
  socket_->async_read_some(boost::asio::buffer(buffer_), h);
}

void RawConnection::Disconnect(StatusPtr status, bool async) {
  if (status->closing()) {
    RawConnTrace << "Already closing";
    return;
  }
  boost::shared_mutex *mut = &status->mutex();
  mut->unlock_shared();
  mut->lock_upgrade();
  mut->unlock_upgrade_and_lock();
  if (status->closing()) {
    RawConnTrace << "Already already closing";
    mut->unlock();
    return;
  }
  status->set_closing();
  mut->unlock();
  send_timer_.reset();
  if (socket_.get()) {
    socket_->close();
    socket_.reset();
  }
  boost::shared_ptr<Connection> conn = connection_;
  connection_.reset();
  if (async) {
    conn->ImplClosed();
  }
}

RawConnection::~RawConnection() {
  VLOG(2) << name() << "~RawConnection";
}

void RawConnection::StartOOBSend(StatusPtr status) {
  WaitHandler h(status, send_timer_, this, &RawConnection::OOBSend);
  send_timer_->Wait(h);
}

void RawConnection::StartOOBRecv(StatusPtr status) {
  ReadHandler h(status, this, &RawConnection::OOBRecv);
  socket_->async_receive(boost::asio::buffer(&heartbeat_, sizeof(heartbeat_)),
                         boost::asio::socket_base::message_out_of_band,
                         h);
}

void RawConnection::OOBSend(StatusPtr status,
                            const boost::system::error_code &e) {
  if (e != boost::asio::error::operation_aborted) {
    char heartbeat = kHeartBeat;
    boost::system::error_code ec;
    int n= socket_->send(boost::asio::buffer(&heartbeat, sizeof(heartbeat)),
                         boost::asio::socket_base::message_out_of_band, ec);
    if (ec || (n != sizeof(heartbeat))) {
      VLOG(2) << name() << " : " << "OOBSend error, n:" << n << " ec: " << ec.message();
      Disconnect(status, true);
      return;
    }
    StartOOBSend(status);
  }
  status->mutex().unlock_shared();
}

void RawConnection::OOBRecv(
    StatusPtr status,
    const boost::system::error_code &e, size_t n) {
  if (e) {
    Disconnect(status, true);
    return;
  }
  StartOOBRecv(status);
  status->mutex().unlock_shared();
}

void RawConnection::HandleRead(StatusPtr status,
                               const boost::system::error_code& e,
                               size_t bytes_transferred) {
  CHECK(status->reading());
  RawConnTrace<< " e:" << e.message() << " bytes: " << bytes_transferred;
  if (e) {
    status->clear_reading();
    Disconnect(status, true);
    return;
  }
  if (!Decode(bytes_transferred)) {
    RawConnTrace << "Decoder error";
    status->clear_reading();
    Disconnect(status, true);
    return;
  }
  ReadHandler h(status, this, &RawConnection::HandleRead);
  socket_->async_read_some(
      boost::asio::buffer(buffer_),
      h);
  status->mutex().unlock_shared();
}

bool RawConnection::ScheduleWrite(StatusPtr status) {
  RawConnTrace;
  boost::mutex::scoped_lock incoming_locker(incoming_mutex_);
  if (status->writting()) {
    RawConnTrace << " : " << "ScheduleWrite but already writting";
    return true;
  }
  status->set_writting();
  RawConnTrace << "duplex_[0] : " << duplex_[0].empty();
  RawConnTrace << "duplex_[1] : " << duplex_[1].empty();
  RawConnTrace << "incoming: " << incoming_index_;
  SwitchIO();
  if (outcoming()->empty()) {
    status->clear_writting();
    RawConnTrace << "No outcoming";
    return true;
  }

  WriteHandler h(status, this, &RawConnection::HandleWrite);
  socket_->async_write_some(
      *outcoming(),
      h);
  return true;
}

void RawConnection::HandleWrite(
    StatusPtr status,
    const boost::system::error_code& e, size_t byte_transferred) {
  CHECK(status->writting());
  RawConnTrace << "e:" << e.message() << " byte_transferred: " << byte_transferred;
  if (e) {
    status->clear_writting();
    Disconnect(status, true);
    return;
  }
  boost::mutex::scoped_lock locker(incoming_mutex_);
  outcoming()->consume(byte_transferred);
  if (!outcoming()->empty()) {
    WriteHandler h(status, this, &RawConnection::HandleWrite);
    socket_->async_write_some(
        *outcoming(),
        h);
  } else {
    RawConnTrace << "outcoming is empty";
    SwitchIO();
    if (!outcoming()->empty()) {
      RawConnTrace << " : " << "outcoming is not empty after SwitchIO, size :" << outcoming()->size();
      WriteHandler h(status, this, &RawConnection::HandleWrite);
      socket_->async_write_some(
          *outcoming(),
          h);
    } else {
      status->clear_writting();
    }
  }
  status->mutex().unlock_shared();
}
#undef RawConnTrace
