#ifndef NET2_CONNECTION_HPP_
#define NET2_CONNECTION_HPP_
#include "base/base.hpp"
#include "glog/logging.h"
#include "server/io_service_pool.hpp"
#include "protobuf/service.h"
class FullDualChannel : virtual public google::protobuf::RpcController,
  virtual public google::protobuf::RpcChannel {
 public:
  virtual void RegisterService(google::protobuf::Service *service) = 0;
  virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) = 0;
  void SetFailed(const string &failed) {
    failed_ = failed;
  }
  bool Failed() const {
    return !failed_.empty();
  }
  string ErrorText() const {
    return failed_;
  }
  void StartCancel() {
  }
  bool IsCanceled() const {
    return false;
  }
  void NotifyOnCancel(google::protobuf::Closure *callback) {
  }
  void Reset() {
    failed_.clear();
  }
 private:
  string failed_;
};

class Connection : public boost::enable_shared_from_this<Connection>, public FullDualChannel {
 public:
  void set_socket(shared_ptr<boost::asio::ip::tcp::socket> socket) {
    socket_ = socket;
  }

  shared_ptr<boost::asio::ip::tcp::socket> socket() const {
    return socket_;
  }
  virtual ~Connection() {}

  virtual void ScheduleRead() = 0;
  virtual Connection *Clone() = 0;
  virtual void ScheduleWrite() = 0;
  virtual void Close() {
    socket_->close();
  }
 protected:
  virtual void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred) = 0;
  /// Handle completion of a write operation.
  virtual void HandleWrite(const boost::system::error_code& e,
                           shared_ptr<Object> resource) = 0;
  shared_ptr<boost::asio::ip::tcp::socket> socket_;
};

typedef shared_ptr<Connection> ConnectionPtr;
/// Represents a single Connection from a client.
template <typename LineFormat,
          typename Handler,
          typename Reply>
class ConnectionImpl : virtual public Connection {
public:
  typedef typename LineFormat::Encoder Encoder;
  typedef typename LineFormat::Parser Parser;
  // ScheduleRead the first asynchronous operation for the Connection.
  void ScheduleRead();

  void ScheduleWrite();

protected:
  class Status {
   public:
    Status() : status_(IDLE) {
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

   private:
    enum InternalStatus {
      IDLE = 0x01,
      READING = 0x01 << 1,
      WRITTING = 0x01 << 2
    };
    int status_;
  };

  /// Handle completion of a read operation.
  virtual void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred);

  /// Handle completion of a write operation.
  virtual void HandleWrite(const boost::system::error_code& e,
                           shared_ptr<Object> resource);

  static const int kBufferSize = 8192;
  typedef boost::array<char, kBufferSize> Buffer;

  Buffer buffer_;
  /// The handler used to process the incoming request.
  Handler handler_;

  /// The incoming request.
  LineFormat lineformat_;

  /// The parser for the incoming request.
  Parser parser_;

  /// The reply to be sent back to the client.
  Reply reply_;

  IOServicePtr io_service_;

  Status status_;
};

template <typename LineFormat, typename Handler, typename Reply>
void ConnectionImpl<LineFormat, Handler, Reply>::ScheduleRead() {
  if (status_.reading()) {
    VLOG(2) << "Alreading in reading status";
    return;
  }
  status_.set_reading();
  VLOG(2) << "ScheduleRead" << " socket open: " << socket_->is_open();
  socket_->async_read_some(boost::asio::buffer(buffer_),
      boost::bind(&Connection::HandleRead, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

template <typename LineFormat, typename Handler, typename Reply>
void ConnectionImpl<LineFormat, Handler, Reply>::ScheduleWrite() {
  if (status_.writting()) {
    VLOG(2) << "Alreading in writting status";
    return;
  }
  status_.set_writting();

  VLOG(2) << "Schedule Write socket open:" << socket_->is_open();
  shared_ptr<Encoder> encoder = reply_.PopEncoder();
  if (encoder.get() == NULL) {
    status_.clear_writting();
    LOG(WARNING) << "Encoder is null";
    return;
  }
  boost::asio::async_write(*socket_.get(), encoder->ToBuffers(),
                           boost::bind(&Connection::HandleWrite, shared_from_this(),
                                       boost::asio::placeholders::error, encoder));
}

template <typename LineFormat, typename Handler, typename Reply>
void ConnectionImpl<
  LineFormat, Handler, Reply>::HandleRead(
      const boost::system::error_code& e,
      size_t bytes_transferred) {
  VLOG(2) << "Handle read, e: " << e.message() << ", bytes: "
          << bytes_transferred << " content: "
          << string(buffer_.data(), bytes_transferred);
  CHECK(status_.reading());
  if (!e) {
    boost::tribool result;
    const char *start = buffer_.data();
    const char *end = start + bytes_transferred;
    const char *p = start;
    while (p < end) {
      boost::tie(result, p) =
        parser_.Parse(
            &lineformat_, p, end);

      if (result) {
        VLOG(2) << "Handle lineformat: size: " << (p - start);
        handler_.HandleLineFormat(lineformat_, this, &reply_);
        ScheduleWrite();
      } else if (!result) {
        VLOG(2) << "Parse error";
        ScheduleWrite();
        break;
      } else {
        VLOG(2) << "Need to read more data";
        break;
      }
    }
    status_.clear_reading();
    ScheduleRead();
  } else {
    status_.clear_reading();
  }

  // If an error occurs then no new asynchronous operations are started. This
  // means that all shared_ptr references to the ConnectionImpl object will
  // disappear and the object will be destroyed automatically after this
  // handler returns. The ConnectionImpl class's destructor closes the socket.
}

template <typename LineFormat, typename Handler, typename Reply>
void ConnectionImpl<
  LineFormat, Handler, Reply>::HandleWrite(
      const boost::system::error_code& e, shared_ptr<Object> encoder) {
    CHECK(status_.writting());
    if (!e) {
      status_.clear_writting();
      ScheduleWrite();
    } else {
      status_.clear_writting();
    }

  // No new asynchronous operations are started. This means that all shared_ptr
  // references to the ConnectionImpl object will disappear and the object will be
  // destroyed automatically after this handler returns. The ConnectionImpl class's
  // destructor closes the socket.
}
#endif // NET2_CONNECTION_HPP_
