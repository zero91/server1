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
              RawConnection *connection,
              boost::intrusive_ptr<Timer> timer,
            void (RawConnection::*member)(
                RawConnection::StatusPtr status,
                const boost::system::error_code&))
    : status_(status), connection_(connection), timer_(timer), member_(member) {
  }
  void operator()(const boost::system::error_code& e) {
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
  boost::intrusive_ptr<Timer> timer_;
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
                             boost::shared_ptr<Connection> connection,
                             int timeout)
  : name_(name),
    timeout_(timeout),
    incoming_index_(0),
    connection_(connection) {
}

void RawConnection::InitSocket(StatusPtr status,
                               boost::asio::ip::tcp::socket *socket) {
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
  send_timer_.reset(new Timer(socket->get_io_service()));
  recv_timer_.reset(new Timer(socket->get_io_service()));
  ExecuteHandler send_handler(status, this, &RawConnection::StartOOBSend);
  ExecuteHandler recv_handler(status, this, &RawConnection::StartOOBRecv);
  socket_->get_io_service().post(send_handler);
  socket_->get_io_service().post(recv_handler);
  status->set_reading();
  ReadHandler h(status, this, &RawConnection::HandleRead);
  socket_->async_read_some(boost::asio::buffer(buffer_), h);
}

void RawConnection::Disconnect(StatusPtr status, bool async) {
  boost::shared_mutex *mut = &status->mutex();
  mut->unlock_shared();
  mut->lock_upgrade();
  mut->unlock_upgrade_and_lock();
  if (status->closing()) {
    RawConnTrace << "Already closing";
    mut->unlock();
    return;
  }
  status->set_closing();
  mut->unlock();
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
  if (async) {
    connection_->ImplClosed();
  }
  connection_.reset();
}

RawConnection::~RawConnection() {
  VLOG(2) << name() << "~RawConnection";
}

void RawConnection::StartOOBSend(StatusPtr status) {
  OOBSend(status, boost::system::error_code());
}

void RawConnection::StartOOBRecv(StatusPtr status) {
  OOBRecv(status, boost::system::error_code(), 0);
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
    send_timer_->expires_from_now(boost::posix_time::milliseconds(timeout_));
    WaitHandler h(status, this, send_timer_, &RawConnection::OOBSend);
    send_timer_->async_wait(h);
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

  OOBWait(status);

  ReadHandler h(status, this, &RawConnection::OOBRecv);
  socket_->async_receive(boost::asio::buffer(&heartbeat_, sizeof(heartbeat_)),
                         boost::asio::socket_base::message_out_of_band,
                         h);
  status->mutex().unlock_shared();
}

void RawConnection::Timeout(
    StatusPtr status,
    const boost::system::error_code &e) {
  if (e != boost::asio::error::operation_aborted) {
    LOG(WARNING) << name() << " : " << "Timeouted";
    Disconnect(status, true);
    return;
  }
  status->mutex().unlock_shared();
}

void RawConnection::OOBWait(StatusPtr status) {
  recv_timer_->expires_from_now(boost::posix_time::milliseconds(
      timeout_ * kRecvDelayFactor));
  WaitHandler h(status, this, recv_timer_, &RawConnection::Timeout);
  recv_timer_->async_wait(h);
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
  OOBWait(status);
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
