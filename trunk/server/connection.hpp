#ifndef NET2_CONNECTION_HPP_
#define NET2_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/io_service_pool.hpp"
class Connection : public enable_shared_from_this<Connection> {
 public:
  void set_io_service(IOServicePtr io_service) {
    io_service_ = io_service;
    socket_.reset(new asio::ip::tcp::socket(*io_service_.get()));
  }

  virtual void Start() = 0;
  asio::ip::tcp::socket *socket() {
    return socket_.get();
  }
  virtual Connection *Clone() const = 0;
 private:
  IOServicePtr io_service_;
 protected:
  /// Handle completion of a read operation.
  virtual void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred) = 0;

  /// Handle completion of a write operation.
  virtual void HandleWrite(const boost::system::error_code& e) = 0;

  scoped_ptr<asio::ip::tcp::socket> socket_;
};
typedef shared_ptr<Connection> ConnectionPtr;

/// Represents a single Connection from a client.
template <typename Request,
          typename RequestHandler,
          typename RequestParser,
          typename Reply>
class ConnectionImpl : public Connection {
public:
  // Start the first asynchronous operation for the Connection.
  void Start();

  Connection* Clone() const {
    return new ConnectionImpl;
  }

private:
  static const int kBufferSize = 8192;
  /// Handle completion of a read operation.
  void HandleRead(const boost::system::error_code& e,
      size_t bytes_transferred);

  /// Handle completion of a write operation.
  void HandleWrite(const boost::system::error_code& e);

  /// The handler used to process the incoming request.
  RequestHandler request_handler_;

  /// Buffer for incoming data.
  array<char, kBufferSize> buffer_;

  /// The incoming request.
  Request request_;

  /// The parser for the incoming request.
  RequestParser request_parser_;

  /// The reply to be sent back to the client.
  Reply reply_;

  IOServicePtr io_service_;
};

template <typename Request, typename RequestHandler,
          typename RequestParser, typename Reply>
void ConnectionImpl<
  Request, RequestHandler, RequestParser, Reply>::Start() {
  socket_->async_read_some(asio::buffer(buffer_),
      bind(&Connection::HandleRead, shared_from_this(),
        asio::placeholders::error,
        asio::placeholders::bytes_transferred));
}

template <typename Request, typename RequestHandler,
          typename RequestParser, typename Reply>
void ConnectionImpl<
  Request, RequestHandler, RequestParser, Reply>::HandleRead(
      const boost::system::error_code& e,
      size_t bytes_transferred) {
  if (!e) {
    tribool result;
    tie(result, tuples::ignore) =
      request_parser_.Parse(
        &request_, buffer_.data(),
        buffer_.data() + bytes_transferred);

    if (result) {
      request_handler_.HandleRequest(request_, &reply_);
      asio::async_write(*socket_.get(), reply_.ToBuffers(),
          bind(&Connection::HandleWrite, shared_from_this(),
            asio::placeholders::error));
    } else if (!result) {
      reply_ = Reply::StockReply(Reply::BAD_REQUEST);
      asio::async_write(*socket_.get(), reply_.ToBuffers(),
          bind(&Connection::HandleWrite, shared_from_this(),
            asio::placeholders::error));
    } else {
      socket_->async_read_some(asio::buffer(buffer_),
          bind(&Connection::HandleRead, shared_from_this(),
            asio::placeholders::error,
            asio::placeholders::bytes_transferred));
    }
  }

  // If an error occurs then no new asynchronous operations are started. This
  // means that all shared_ptr references to the ConnectionImpl object will
  // disappear and the object will be destroyed automatically after this
  // handler returns. The ConnectionImpl class's destructor closes the socket.
}

template <typename Request, typename RequestHandler,
          typename RequestParser, typename Reply>
void ConnectionImpl<
  Request, RequestHandler, RequestParser, Reply>::HandleWrite(
      const boost::system::error_code& e) {
  if (!e) {
    // Initiate graceful ConnectionImpl closure.
    boost::system::error_code ignored_ec;
    socket_->shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
  }

  // No new asynchronous operations are started. This means that all shared_ptr
  // references to the ConnectionImpl object will disappear and the object will be
  // destroyed automatically after this handler returns. The ConnectionImpl class's
  // destructor closes the socket.
}
#endif // NET2_CONNECTION_HPP_
