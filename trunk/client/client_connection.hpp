#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP
template <typename LineFormat, typename LineFormatParser>
class ClientConnection {
 public:
  typedef boost::function1<void, const LineFormat *> Listener;
  ClientConnection(IOServicePtr io_service)
    : io_service_(io_service),
      socket_(*io_service.get()) {
  }

  void set_listener(const Listener &listener) {
    listener_ = listener;
  }

  bool IsConnected() const {
    return socket_.is_open();
  }

  bool Connect(const string &server, const string &port) {
    boost::asio::ip::tcp::resolver::query query(server, port);
    boost::asio::ip::tcp::resolver resolver(*io_service_.get());
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator =
      resolver.resolve(query);
    boost::asio::ip::tcp::resolver::iterator end;
    // Try each endpoint until we successfully establish a connection.
    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end) {
      socket_.close();
      socket_.connect(*endpoint_iterator++, error);
    }
    if (error) {
      LOG(WARNING) << ":fail to connect, error:"  << error.message();
      return false;
    }
    socket_.async_read_some(
        boost::asio::buffer(buffer_),
        boost::bind(&ClientConnection::HandleRead, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    return true;
  }
  void Send(const ConstBufferVector &buffers) {
    boost::asio::async_write(
        socket_, buffers,
        boost::bind(&ClientConnection::HandleWrite, this,
                    boost::asio::placeholders::error));
  }
 private:
  void HandleWrite(const boost::system::error_code &err) {
    if (err) {
      LOG(WARNING) << "Write fail";
      // failed.
      listener_(NULL);
    }
  }
  void HandleRead(const boost::system::error_code& e,
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
        listener_(&lineformat_);
      } else if (!result) {
        LOG(WARNING) << "Error line format";
        listener_(NULL);
        return;
      } else {
        LOG(INFO) << "Continue to read";
        socket_.async_read_some(
            boost::asio::buffer(buffer_),
            boost::bind(&ClientConnection::HandleRead, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
      }
    } else {
      LOG(WARNING) << "read error: " << e.message();
      listener_(NULL);
      return;
    }
  }
 protected:
  static const int kBufferSize = 8192;
  IOServicePtr io_service_;
  boost::asio::ip::tcp::socket socket_;
  /// Buffer for incoming data.
  boost::array<char, kBufferSize> buffer_;
  LineFormatParser lineformat_parser_;
  LineFormat lineformat_;
  Listener listener_;
};
#endif  // CLIENT_CONNECTION_HPP


