#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP
#include "server/protobuf_connection.hpp"
class ClientConnection : public ProtobufConnection {
 public:
  ClientConnection(const string &server, const string &port)
    : ProtobufConnection(), io_service_pool_(1), server_(server), port_(port) {
      VLOG(2) << "Constructor client connection";
  }

  bool Connect() {
    if (IsConnected()) {
      LOG(WARNING) << "Connect but IsConnected";
      return true;
    }
    io_service_pool_.Start();
    boost::asio::ip::tcp::resolver::query query(server_, port_);
    boost::asio::ip::tcp::resolver resolver(*io_service_pool_.get_io_service());
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator(
        resolver.resolve(query));
    boost::asio::ip::tcp::resolver::iterator end;
    // Try each endpoint until we successfully establish a connection.
    boost::system::error_code error = boost::asio::error::host_not_found;
    shared_ptr<boost::asio::ip::tcp::socket> socket(
        new boost::asio::ip::tcp::socket(*io_service_pool_.get_io_service()));
    while (error && endpoint_iterator != end) {
      socket->close();
      socket->connect(*endpoint_iterator++, error);
    }
    if (error) {
      LOG(WARNING) << ":fail to connect, error:"  << error.message();
      return false;
    }
    this->set_io_service(io_service_pool_.get_io_service());
    this->set_socket(socket);
    return true;
  }
  void Disconnect() {
    Close();
    io_service_pool_.Stop();
  }
 private:
  IOServicePool io_service_pool_;
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
