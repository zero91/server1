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
  static const int kBufferSize = 8192;
  typedef boost::array<char, kBufferSize> Buffer;
  virtual void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred, shared_ptr<Buffer> buffer) = 0;
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
  /// Handle completion of a read operation.
  virtual void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred, shared_ptr<Buffer> buffer);

  /// Handle completion of a write operation.
  virtual void HandleWrite(const boost::system::error_code& e,
                           shared_ptr<Object> resource);

  /// The handler used to process the incoming request.
  Handler handler_;

  /// The incoming request.
  LineFormat lineformat_;

  /// The parser for the incoming request.
  Parser parser_;

  /// The reply to be sent back to the client.
  Reply reply_;

  IOServicePtr io_service_;
};

template <typename LineFormat, typename Handler, typename Reply>
void ConnectionImpl<LineFormat, Handler, Reply>::ScheduleRead() {
  VLOG(2) << "ScheduleRead" << " open: " << socket_->is_open();
  shared_ptr<Buffer> buffer(new Buffer);
  socket_->async_read_some(boost::asio::buffer(*buffer.get()),
      boost::bind(&Connection::HandleRead, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred, buffer));
}

template <typename LineFormat, typename Handler, typename Reply>
void ConnectionImpl<LineFormat, Handler, Reply>::ScheduleWrite() {
  VLOG(2) << "Schedule Write";
  shared_ptr<Encoder> encoder = reply_.PopEncoder();
  if (encoder.get() == NULL) {
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
      size_t bytes_transferred, shared_ptr<Buffer> buffer) {
  VLOG(2) << "Handle read, e: " << e.message() << ", bytes: "
          << bytes_transferred << " content: "
          << string(buffer->data(), bytes_transferred);
  if (!e) {
    boost::tribool result;
    const char *start = buffer->data();
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
    ScheduleRead();
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
  if (!e) {
    ScheduleWrite();
  }

  // No new asynchronous operations are started. This means that all shared_ptr
  // references to the ConnectionImpl object will disappear and the object will be
  // destroyed automatically after this handler returns. The ConnectionImpl class's
  // destructor closes the socket.
}
#endif // NET2_CONNECTION_HPP_
