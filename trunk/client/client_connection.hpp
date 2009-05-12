#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP
#include "server/protobuf_connection.hpp"
class ClientConnection : public ProtobufConnection {
 public:
  ClientConnection(IOServicePtr io_service,
                   const string &server, const string &port)
    : io_service_(io_service), server_(server), port_(port) {
  }

  bool Connect() {
    boost::asio::ip::tcp::resolver::query query(server_, port_);
    boost::asio::ip::tcp::resolver resolver(*io_service_.get());
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator(
        resolver.resolve(query));
    boost::asio::ip::tcp::resolver::iterator end;
    // Try each endpoint until we successfully establish a connection.
    boost::system::error_code error = boost::asio::error::host_not_found;
    shared_ptr<boost::asio::ip::tcp::socket> socket(
        new boost::asio::ip::tcp::socket(*io_service_.get()));
    while (error && endpoint_iterator != end) {
      socket->close();
      socket->connect(*endpoint_iterator++, error);
    }
    if (error) {
      LOG(WARNING) << ":fail to connect, error:"  << error.message();
      return false;
    }
    this->set_io_service(io_service_);
    this->set_socket(socket);
    return true;
  }
 private:
  IOServicePtr io_service_;
  static const int kBufferSize = 8192;
  string server_, port_;
};

class RpcController : public google::protobuf::RpcController {
 public:
  void Reset() {
    failed_.clear();
  }
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
 private:
  string failed_;
};
#endif  // CLIENT_CONNECTION_HPP


