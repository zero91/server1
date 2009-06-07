// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP
#include <boost/thread/shared_mutex.hpp>
#include "server/protobuf_connection.hpp"
#include "server/io_service_pool.hpp"
class ClientConnection : public ProtobufConnection {
 public:
  ClientConnection(const string &name, const string &server, const string &port)
    : ProtobufConnection(server + "." + port + "." + name),
      io_service_pool_(name + ".IOService", 1, kClientThreadPoolSize),
      server_(server), port_(port), out_io_service_pool_(NULL) {
      VLOG(2) << "Constructor client connection:" << name;
  }

  bool Connect();

  void Disconnect() {
    Connection::Disconnect();
    if (out_io_service_pool_ == NULL) {
      io_service_pool_.Stop();
    }
    VLOG(2) << name() << " Disconnected";
  }
  void set_io_service_pool(IOServicePool *io_service_pool) {
    out_io_service_pool_ = io_service_pool;
    if (io_service_pool_.IsRunning()) {
      io_service_pool_.Stop();
    }
  }
  ~ClientConnection() {
    CHECK(!IsConnected());
    if (io_service_pool_.IsRunning()) {
      io_service_pool_.Stop();
    }
    VLOG(2) << "~ClientConnection";
  }
 private:
  boost::asio::io_service &GetIOService() {
    if (out_io_service_pool_) {
      return out_io_service_pool_->get_io_service();
    }
    return io_service_pool_.get_io_service();
  }

  static const int kClientThreadPoolSize = 1;
  IOServicePool io_service_pool_;
  IOServicePool *out_io_service_pool_;
  string server_, port_;
};
#endif  // CLIENT_CONNECTION_HPP
