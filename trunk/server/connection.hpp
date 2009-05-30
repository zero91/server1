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
#include "base/allocator.hpp"
#include "glog/logging.h"
#include "server/meta.pb.h"
#include "protobuf/service.h"
#include "boost/thread.hpp"
#include "server/shared_const_buffers.hpp"
#include "boost/signals2/signal.hpp"
class Connection;
class ConnectionStatus {
 public:
  ConnectionStatus() : status_(IDLE), reader_(0), writter_(0), handler_(0) {
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

  void set_shutdown() {
    status_ |= SHUTDOWN;
  }

  bool shutdown() {
    return status_ & SHUTDOWN;
  }

  void set_closed() {
    status_ |= CLOSED;
  }

  bool closed() const {
    return status_ & CLOSED;
  }

  bool closing() const {
    return status_ & CLOSING;
  }

  int status() const {
    return status_;
  }

  void DecreaseReaderCounter() {
    --reader_;
  }

  void IncreaseReaderCounter() {
    ++reader_;
  }
  int reader() const {
    return reader_;
  }

  void DecreaseWritterCounter() {
    --writter_;
  }
  void IncreaseWritterCounter() {
    ++writter_;
  }
  int writter() const {
    return writter_;
  }

  void DecreaseHandlerCounter() {
    --handler_;
  }
  void IncreaseHandlerCounter() {
    ++handler_;
  }
  int handler() const {
    return handler_;
  }

  class ScopedExit {
   public:
    ScopedExit(Connection *connection, boost::shared_ptr<ConnectionStatus> status)
      : connection_(connection), status_(status) {
    }
    inline ~ScopedExit();
   private:
    Connection *connection_;
    boost::shared_ptr<ConnectionStatus> status_;
  };
 private:
  enum InternalConnectionStatus {
    IDLE = 0x01,
    READING = 0x01 << 1,
    WRITTING = 0x01 << 2,
    CLOSING = 0x01 << 3,
    SHUTDOWN = 0x01 << 4,
    CLOSED = 0x01 << 5,
  };
  mutable int status_;
  mutable int reader_, writter_, handler_;
  Connection *connection_;
};

class Connection {
 public:
  Connection() : status_(new ConnectionStatus) {
  }
  void Close() {
    if (status_->closing()) {
      VLOG(2) << " Call Close but already in closing";
      return;
    }
    if (!socket_.get()) {
      VLOG(2) << name() << " socket is null, may closed";
      return;
    }
    socket_->get_io_service().post(boost::bind(&Connection::InternalClose, status_, this));
  }

  void set_socket(boost::asio::ip::tcp::socket *socket) {
    socket_.reset(socket);
    boost::asio::socket_base::keep_alive option(true);
    socket_->set_option(option);
  }

  void set_executor(Executor *executor) {
    executor_ = executor;
  }

  Executor *executor() const {
    return executor_;
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
    VLOG(2) << "Distroy connection: " << this;
  }
 protected:
  inline virtual void Cleanup();
  inline void Run(const boost::function0<void> &f);
  static inline void InternalClose(boost::shared_ptr<ConnectionStatus> status, Connection *connection);
  inline void Shutdown();
  inline void RunDone();
  virtual void HandleRead(const boost::system::error_code& e, size_t bytes_transferred) = 0;
  virtual void HandleWrite(const boost::system::error_code& e, size_t byte_transferred) = 0;
  scoped_ptr<boost::asio::ip::tcp::socket> socket_;
  Executor *executor_;
  string name_;
  boost::signals2::signal<void()> close_signal_;
  boost::shared_ptr<ConnectionStatus> status_;
  friend class ConnectionReadHandler;
  friend class ConnectionWriteHandler;
  friend class ConnectionStatus::ScopedExit;
};

class ConnectionReadHandler {
 public:
  ConnectionReadHandler(Connection* connection, boost::shared_ptr<ConnectionStatus> status)
    : connection_(connection), status_(status) {
  }
  void operator()(const boost::system::error_code &e, size_t bytes_transferred) {
    VLOG(2) << "ConnectionReadHandler e: " << e.message() << " bytes: " << bytes_transferred << " status: " << status_->status();
    ConnectionStatus::ScopedExit exiter(connection_, status_);
    status_->DecreaseReaderCounter();
    if (!e) {
      if (!status_->closed()) {
        connection_->HandleRead(e, bytes_transferred);
      } else {
        status_->clear_reading();
        VLOG(2) << "non error but connection is closed";
      }
    } else {
      if (!status_->closed()) {
        VLOG(2) << "error then closing " << connection_->name();
        status_->clear_reading();
        connection_->Close();
      } else {
        status_->clear_reading();
        VLOG(2) << "non error but connection is closed";
      }
    }
  }
 private:
  Connection* connection_;
  boost::shared_ptr<ConnectionStatus> status_;
};

class ConnectionWriteHandler {
 public:
  ConnectionWriteHandler(Connection* connection, boost::shared_ptr<ConnectionStatus> status)
    : connection_(connection), status_(status) {
  }
  void operator() (const boost::system::error_code &e, size_t byte_transferred) {
    VLOG(2) << "ConnectionWriteHandler e: " << e.message() << " bytes: " << byte_transferred;
    ConnectionStatus::ScopedExit exiter(connection_, status_);
    status_->DecreaseWritterCounter();
    if (!e) {
      if (!status_->closed()) {
        connection_->HandleWrite(e, byte_transferred);
      } else {
        status_->clear_writting();
        VLOG(2) << "non error but connection is already closed";
      }
    } else {
      if (!status_->closed()) {
        VLOG(2) << "error then closing " << connection_->name();
        status_->clear_writting();
        connection_->Close();
      } else {
        status_->clear_reading();
        VLOG(2) << "non error but connection is closed";
      }
    }
  }
 private:
  Connection* connection_;
  boost::shared_ptr<ConnectionStatus> status_;
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

ConnectionStatus::ScopedExit::~ScopedExit() {
  if (status_->closed()) {
    VLOG(2) << "~ScopedExit connection closed";
    return;
  }
  VLOG(2) << connection_->name() << " reader: " << status_->reader() << " writer: " << status_->writter() << " handler: " << status_->handler() << " status: " << status_->status();
  if (status_->reader() == 0 && status_->handler() == 0 && status_->writter() == 0 && status_->shutdown()) {
    VLOG(2) << "Shutdown : " << connection_->name();
    connection_->Shutdown();
  }
}

void Connection::InternalClose(boost::shared_ptr<ConnectionStatus> status, Connection *connection) {
  if (status->closing()) {
    VLOG(2) << "Call InternalClose but already in closing";
    return;
  }
  status->set_closing();
  status->set_shutdown();
  ConnectionStatus::ScopedExit exiter(connection, status);
  VLOG(2) << connection->name() << " : " << "InternalClose";
  boost::system::error_code ignored_ec;
  connection->socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ignored_ec);
}

void Connection::Shutdown() {
  if (status_->closed()) {
    VLOG(2) << "Call Shutdown but already closed";
  }
  status_->set_closed();
  boost::system::error_code ignored_ec;
  socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored_ec);
  socket_->cancel();
  socket_->get_io_service().post(boost::bind(&Connection::Cleanup, this));
}

void Connection::Cleanup() {
  VLOG(2) << "Connection::Cleanup";
  socket_->close();
  socket_.reset();
  VLOG(2) << "Cleanup, num_slots: " << close_signal_.num_slots();
  close_signal_();
  delete this;
}

void Connection::Run(const boost::function0<void> &f) {
  VLOG(2) << name() << " Run";
  f();
  socket_->get_io_service().post(boost::bind(&Connection::RunDone, this));
}

void Connection::RunDone() {
  VLOG(2) << name() << " : RunDone";
  ConnectionStatus::ScopedExit exiter(this, status_);
  status_->DecreaseHandlerCounter();
}

template <class Decoder>
void ConnectionImpl<Decoder>::HandleRead(const boost::system::error_code& e,
                                         size_t bytes_transferred) {
  VLOG(2) << "Handle read, e: " << e.message() << ", bytes: "
    << bytes_transferred;
  CHECK(status_->reading());
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
      Executor *this_executor = this->executor();
      boost::function0<void> handler_run = boost::bind(&ConnectionImpl<Decoder>::Handle, this, shared_decoder);
      boost::function0<void> h = boost::bind(&Connection::Run, this, handler_run);
      status_->IncreaseHandlerCounter();
      if (this_executor == NULL) {
        h();
      } else {
        // This is executed in another thread.
        this_executor->Run(h);
      }
    } else if (!result) {
      VLOG(2) << name() << " : " << "Parse error";
      status_->clear_reading();
      InternalScheduleRead();
      break;
    } else {
      VLOG(2) << name() << " : " << "Need to read more data";
      status_->clear_reading();
      InternalScheduleRead();
      return;
    }
  }
  VLOG(2) << name() << " : " << this << "ScheduleRead After reach the end status: " << status_->status() <<" : " << this;
  status_->clear_reading();
  ScheduleRead();
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleRead() {
  if (status_->closing()) {
    VLOG(2) << "Call ScheduleRead but is closing";
    return;
  }
  VLOG(2) << name() << " : " << " status: " << status_->status();
  if (status_->reading()) {
    VLOG(2) << name() << " : " << "Alreading in reading status";
    return;
  }
  if (!socket_.get()) {
    VLOG(2) << name() << " : " << " ScheduleRead but socket is null";
    return;
  }
  socket_->get_io_service().post(boost::bind(&ConnectionImpl<Decoder>::InternalScheduleRead, this));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::InternalScheduleRead() {
  VLOG(2) << name() << " : " << " Internal ScheduleRead status: " << status_->status();
  if (status_->reading()) {
    VLOG(2) << name() << " : " << "Alreading in reading status";
    return;
  }
  if (!socket_.get()) {
    VLOG(2) << name() << " : " << " ScheduleRead but socket is null";
    return;
  }
  status_->set_reading();
  status_->IncreaseReaderCounter();
  socket_->async_read_some(boost::asio::buffer(buffer_), ConnectionReadHandler(this, status_));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleWrite() {
  if (status_->closing()) {
    VLOG(2) << " ScheduleWrite but is closing";
    return;
  }
  VLOG(2) << name() << " : " << " status: " << status_->status();
  if (status_->writting()) {
    VLOG(2) << name() << " : " << "Alreading in writting status";
    return;
  }
  if (!socket_.get()) {
    VLOG(2) << name() << " : " << " ScheduleWrite but socket is null";
    return;
  }
  socket_->get_io_service().post(boost::bind(&ConnectionImpl<Decoder>::InternalScheduleWrite, this));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::InternalScheduleWrite() {
  VLOG(2) << name() << " : " << this << " InternalScheduleWrite" << " status: " << status_->status();
  if (status_->writting()) {
    VLOG(2) << name() << " : " << "Alreading in writting status";
    return;
  }
  if (!socket_.get()) {
    VLOG(2) << name() << " : " << " InternalScheduleWrite but socket is null";
    return;
  }
  status_->set_writting();
  VLOG(2) << name() << " : " << " status: " << status_->status() << " " << this;
  VLOG(2) << "duplex_[0] : " << duplex_[0].empty();
  VLOG(2) << "duplex_[1] : " << duplex_[1].empty();
  VLOG(2) << "incoming: " << incoming_index_;

  {
    boost::mutex::scoped_lock locker(incoming_mutex_);
    CHECK(outcoming()->empty());
    // Switch the working vector.
    incoming_index_ = 1 - incoming_index_;
  }
  VLOG(2) << name() << " : " << " Internal Schedule Write socket open:" << socket_->is_open();
  if (outcoming()->empty()) {
    status_->clear_writting();
    VLOG(2) << "No outcoming";
    return;
  }
  status_->IncreaseWritterCounter();
  socket_->async_write_some(*outcoming(), ConnectionWriteHandler(this, status_));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::HandleWrite(const boost::system::error_code& e, size_t byte_transferred) {
  VLOG(2) << name() << " : " << this << " HandleWrite bytes: " << byte_transferred << " status: " << status_->status();
  CHECK(status_->writting());
  if (!e) {
    outcoming()->consume(byte_transferred);
    if (!outcoming()->empty()) {
      VLOG(2) << "outcoming is not empty";
      status_->IncreaseWritterCounter();
      socket_->async_write_some(*outcoming(), ConnectionWriteHandler(this, status_));
    } else {
      VLOG(2) << "outcoming is empty";
      outcoming()->clear();
      status_->clear_writting();
      InternalScheduleWrite();
    }
  } else {
    status_->clear_writting();
    VLOG(2) << name() << " : " << "Write error, clear status and return";
  }
}
#endif // NET2_CONNECTION_HPP_
