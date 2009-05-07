#ifndef NET2_CONNECTION_HPP_
#define NET2_CONNECTION_HPP_
#include "base/base.hpp"
#include "glog/logging.h"
#include "server/io_service_pool.hpp"
class Connection : public boost::enable_shared_from_this<Connection> {
 public:
  void set_io_service(IOServicePtr io_service) {
    io_service_ = io_service;
    socket_.reset(new boost::asio::ip::tcp::socket(*io_service_.get()));
  }
  virtual ~Connection() {}

  virtual void Start() = 0;
  boost::asio::ip::tcp::socket *socket() {
    return socket_.get();
  }
  virtual Connection *Clone() = 0;
 private:
  IOServicePtr io_service_;
 protected:
  /// Handle completion of a read operation.
  virtual void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred) = 0;

  /// Handle completion of a write operation.
  virtual void HandleWrite(const boost::system::error_code& e) = 0;

  scoped_ptr<boost::asio::ip::tcp::socket> socket_;
};
typedef shared_ptr<Connection> ConnectionPtr;

/// Represents a single Connection from a client.
template <typename LineFormat,
          typename LineFormatHandler,
          typename LineFormatParser,
          typename Reply>
class ConnectionImpl : public Connection {
public:
  // Start the first asynchronous operation for the Connection.
  void Start();

  Connection* Clone() {
    ConnectionImpl *connect = new ConnectionImpl;
    // To get the handler table.
    connect->lineformat_handler_ = lineformat_handler_;
    return connect;
  }

protected:
  static const int kBufferSize = 8192;
  /// Handle completion of a read operation.
  virtual void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred);

  /// Handle completion of a write operation.
  virtual void HandleWrite(const boost::system::error_code& e);

  /// The handler used to process the incoming request.
  LineFormatHandler lineformat_handler_;

  /// Buffer for incoming data.
  boost::array<char, kBufferSize> buffer_;

  /// The incoming request.
  LineFormat lineformat_;

  /// The parser for the incoming request.
  LineFormatParser lineformat_parser_;

  /// The reply to be sent back to the client.
  Reply reply_;

  IOServicePtr io_service_;
};

template <typename LineFormat, typename LineFormatHandler,
          typename LineFormatParser, typename Reply>
void ConnectionImpl<
  LineFormat, LineFormatHandler, LineFormatParser, Reply>::Start() {
  VLOG(2) << "Connection start";
  socket_->async_read_some(boost::asio::buffer(buffer_),
      boost::bind(&Connection::HandleRead, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

template <typename LineFormat, typename LineFormatHandler,
          typename LineFormatParser, typename Reply>
void ConnectionImpl<
  LineFormat, LineFormatHandler, LineFormatParser, Reply>::HandleRead(
      const boost::system::error_code& e,
      size_t bytes_transferred) {
  VLOG(2) << "Handle read, e: " << e.message() << ", bytes: "
          << bytes_transferred;
  if (!e) {
    boost::tribool result;
    boost::tie(result, boost::tuples::ignore) =
      lineformat_parser_.Parse(
        &lineformat_, buffer_.data(),
        buffer_.data() + bytes_transferred);

    if (result) {
      lineformat_handler_.HandleLineFormat(lineformat_, &reply_);
      boost::asio::async_write(*socket_.get(), reply_.ToBuffers(),
          boost::bind(&Connection::HandleWrite, shared_from_this(),
            boost::asio::placeholders::error));
    } else if (!result) {
      boost::asio::async_write(*socket_.get(), reply_.ToBuffers(),
          boost::bind(&Connection::HandleWrite, shared_from_this(),
            boost::asio::placeholders::error));
    } else {
      socket_->async_read_some(boost::asio::buffer(buffer_),
          boost::bind(&Connection::HandleRead, shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }
  }

  // If an error occurs then no new asynchronous operations are started. This
  // means that all shared_ptr references to the ConnectionImpl object will
  // disappear and the object will be destroyed automatically after this
  // handler returns. The ConnectionImpl class's destructor closes the socket.
}

template <typename LineFormat, typename LineFormatHandler,
          typename LineFormatParser, typename Reply>
void ConnectionImpl<
  LineFormat, LineFormatHandler, LineFormatParser, Reply>::HandleWrite(
      const boost::system::error_code& e) {
  if (!e) {
    if (!reply_.IsRunning()) {
      // Initiate graceful ConnectionImpl closure.
      boost::system::error_code ignored_ec;
      socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                        ignored_ec);
    }
  }

  // No new asynchronous operations are started. This means that all shared_ptr
  // references to the ConnectionImpl object will disappear and the object will be
  // destroyed automatically after this handler returns. The ConnectionImpl class's
  // destructor closes the socket.
}
#endif // NET2_CONNECTION_HPP_
