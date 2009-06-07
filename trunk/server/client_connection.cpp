// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include "server/client_connection.hpp"
bool ClientConnection::Connect() {
  if (IsConnected()) {
    LOG(WARNING) << "Connect but IsConnected";
    return true;
  }
  if (out_io_service_pool_ == NULL) io_service_pool_.Start();
  boost::asio::ip::tcp::resolver::query query(server_, port_);
  boost::asio::ip::tcp::resolver resolver(GetIOService());
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator(
      resolver.resolve(query));
  boost::asio::ip::tcp::resolver::iterator end;
  // Try each endpoint until we successfully establish a connection.
  boost::system::error_code error = boost::asio::error::host_not_found;
  boost::asio::ip::tcp::socket *socket =
    new boost::asio::ip::tcp::socket(GetIOService());
  while (error && endpoint_iterator != end) {
    socket->close();
    socket->connect(*endpoint_iterator++, error);
  }
  if (error) {
    delete socket;
    LOG(WARNING) << ":fail to connect, error:"  << error.message();
    return false;
  }
  CHECK(impl_ == NULL);
  Attach(this, socket);
  return true;
}
