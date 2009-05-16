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
class Connection;
class ConnectionStatus {
 public:
  ConnectionStatus() : status_(IDLE) {
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
    IDLE = 0x01,
    READING = 0x01 << 1,
    WRITTING = 0x01 << 2,
    CLOSING = 0x01 << 3
  };
  int status_;
};

class Connection : public Executor {
 public:
  virtual ~Connection() {
    VLOG(2) << name() << " : " << " destroy.";
  }
  void Close() {
    if (status_->closing()) {
      VLOG(2) << name() << " already in closing";
      return;
    }
    if (!socket_.get()) {
      VLOG(2) << name() << " socket is null, may closed";
      return;
    }
    if (socket_.get()) {
      socket_->get_io_service().post(boost::bind(&Connection::InternalClose, this));
    }
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

  void set_close_handler(const boost::function0<void> &h) {
    close_handler_ = h;
  }

  void set_connection_status(boost::shared_ptr<ConnectionStatus> status) {
    status_ = status;
  }
  virtual bool IsConnected() {
    return socket_ && socket_->is_open();
  }

  virtual Connection* Clone() = 0;
  virtual void ScheduleRead() = 0;
  virtual void ScheduleWrite() = 0;
 protected:
  void InternalClose() {
    if (status_->closing()) {
      VLOG(2) << name() << " already in closing";
      return;
    }
    status_->set_closing();
    VLOG(2) << name() << " : " << "Connection Distroy " << this;
    boost::system::error_code ignored_ec;
    socket_->cancel();
    socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
    socket_->close();
    socket_.reset();
    if (!close_handler_.empty()) {
      close_handler_();
    }
  }
  virtual void HandleRead(const boost::system::error_code& e, size_t bytes_transferred) = 0;
  virtual void HandleWrite(const boost::system::error_code& e, size_t byte_transferred) = 0;
  scoped_ptr<boost::asio::ip::tcp::socket> socket_;
  Executor *executor_;
  string name_;
  boost::function0<void> close_handler_;
  boost::shared_ptr<ConnectionStatus> status_;
  friend class ConnectionReadHandler;
  friend class ConnectionWriteHandler;
};
class ConnectionReadHandler {
 public:
  ConnectionReadHandler(Connection* connection, boost::shared_ptr<ConnectionStatus> status)
    : connection_(connection), status_(status) {
  }
  virtual void operator()(const boost::system::error_code &e, size_t bytes_transferred) {
    VLOG(2) << "ConnectionWriteHandler e: " << e.message() << " bytes: " << bytes_transferred;
    if (!e) {
      if (!status_->closing()) {
        connection_->HandleRead(e, bytes_transferred);
      } else {
        VLOG(2) << "non error but connection is already closing";
      }
    } else {
      if (!status_->closing()) {
        status_->set_closing();
        connection_->Close();
      } else {
        VLOG(2) << "error but connection is already closing";
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
    if (!e) {
      if (!status_->closing()) {
        connection_->HandleWrite(e, byte_transferred);
      } else {
        VLOG(2) << "non error but connection is already closing";
      }
    } else {
      if (!status_->closing()) {
        status_->set_closing();
        connection_->Close();
      } else {
        VLOG(2) << "error but connection is already closing";
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
 private:
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

template <class Decoder>
void ConnectionImpl<Decoder>::HandleRead(const boost::system::error_code& e,
                                         size_t bytes_transferred) {
  VLOG(2) << name() << " : " << this << " ScheduleRead handle read connection";
  VLOG(2) << "Handle read, e: " << e.message() << ", bytes: "
    << bytes_transferred << " content: "
    << string(buffer_.data(), bytes_transferred);
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
      VLOG(2) << name() << " : " << " use count" << " status: " << status_->status() <<" : " << this;
      boost::shared_ptr<const Decoder> shared_decoder(decoder_);
      decoder_.reset(new Decoder);
      Executor *this_executor = this->executor();
      VLOG(2) << name() << " : " << " use count" << " status: " << status_->status() <<" : " << this;
      VLOG(2) << name() << " : " << " use count" << " status: " << status_->status() <<" : " << this;
      /*
         if (this_executor.get() == NULL) {
         Handle(shared_decoder);
         } else {
      // This is executed in another thread.
      boost::function0<void> handler = boost::bind(&ConnectionImpl<Decoder>::Handle, this, shared_decoder);
      this_executor->Run(boost::bind(
          &Connection::Run, shared_from_this(), handler));
          }
          */
      Handle(shared_decoder);
      VLOG(2) << name() << " : " << this << " ScheduleRead execute use count" << " status: " << status_->status() <<" : " << this;
    } else if (!result) {
      VLOG(2) << name() << " : " << "Parse error";
      status_->clear_reading();
      ScheduleRead();
      break;
    } else {
      VLOG(2) << name() << " : " << "Need to read more data";
      status_->clear_reading();
      ScheduleRead();
      return;
    }
  }
  VLOG(2) << name() << " : " << this << "ScheduleRead After reach the end use count" << " status: " << status_->status() <<" : " << this;
  status_->clear_reading();
  ScheduleRead();
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleRead() {
  VLOG(2) << name() << " : " << " status: " << status_->status();
  if (status_->reading()) {
    VLOG(2) << name() << " : " << "Alreading in reading status";
    return;
  }
  if (status_->closing()) {
    VLOG(2) << name() << " : " << " ScheduleRead but is closing";
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
  VLOG(2) << name() << " : " << " ScheduleRead status: " << status_->status();
  if (status_->reading()) {
    VLOG(2) << name() << " : " << "Alreading in reading status";
    return;
  }
  if (status_->closing()) {
    VLOG(2) << name() << " : " << " ScheduleRead but is closing";
    return;
  }
  if (!socket_.get()) {
    VLOG(2) << name() << " : " << " ScheduleRead but socket is null";
    return;
  }
  status_->set_reading();
  socket_->async_read_some(boost::asio::buffer(buffer_), ConnectionReadHandler(this, status_));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::ScheduleWrite() {
  VLOG(2) << name() << " : " << " status: " << status_->status();
  if (status_->closing()) {
    VLOG(2) << name() << " : " << " ScheduleWrite but is closing";
    return;
  }
  if (status_->writting()) {
    VLOG(2) << name() << " : " << "Alreading in writting status";
    return;
  }
  if (!socket_.get()) {
    VLOG(2) << name() << " : " << " ScheduleWrite but socket is null";
    return;
  }
  VLOG(2) << name() << " : " << " status: " << status_->status() << " " << this;
  socket_->get_io_service().post(boost::bind(&ConnectionImpl<Decoder>::InternalScheduleWrite, this));
}

template <typename Decoder>
void ConnectionImpl<Decoder>::InternalScheduleWrite() {
  VLOG(2) << name() << " : " << " status: " << status_->status();
  if (status_->closing()) {
    VLOG(2) << name() << " : " << " InternalScheduleWrite but is closing";
    return;
  }
  if (status_->writting()) {
    VLOG(2) << name() << " : " << "Alreading in writting status";
    return;
  }
  if (!socket_.get()) {
    VLOG(2) << name() << " : " << " InternalScheduleWrite but socket is null";
    return;
  }
  VLOG(2) << name() << " : " << " status: " << status_->status() << " " << this;
  VLOG(2) << "duplex_[0] : " << duplex_[0].empty();
  VLOG(2) << "duplex_[1] : " << duplex_[1].empty();
  VLOG(2) << "incoming: " << incoming_index_;
  status_->set_writting();

  {
    boost::mutex::scoped_lock locker(incoming_mutex_);
    CHECK(outcoming()->empty());
    // Switch the working vector.
    incoming_index_ = 1 - incoming_index_;
  }
  VLOG(2) << name() << " : " << "Schedule Write socket open:" << socket_->is_open();
  if (outcoming()->empty()) {
    status_->clear_writting();
    LOG(WARNING) << "No outcoming";
    return;
  }
  socket_->async_write_some(*outcoming(), ConnectionWriteHandler(this, status_));
  VLOG(2) << name() << " : " << this << " InternalScheduleWrite" << " status: " << status_->status();
}

template <typename Decoder>
void ConnectionImpl<Decoder>::HandleWrite(const boost::system::error_code& e, size_t byte_transferred) {
  if (status_->closing()) {
    VLOG(2) << name() << " : " << " ScheduleWrite but is closing";
    return;
  }
  VLOG(2) << name() << " : " << this << " HandleWrite bytes: " << byte_transferred << " status: " << status_->status();
  CHECK(status_->writting());
  if (!e) {
    outcoming()->consume(byte_transferred);
    if (!outcoming()->empty()) {
      VLOG(2) << "outcoming is not empty";
      socket_->async_write_some(*outcoming(), ConnectionWriteHandler(this, status_));
    } else {
      VLOG(2) << "outcoming is empty";
      outcoming()->clear();
      status_->clear_writting();
      ScheduleWrite();
    }
  } else {
    status_->clear_writting();
    VLOG(2) << name() << " : " << "Write error, clear status and return";
  }
}
#endif // NET2_CONNECTION_HPP_
