/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */



#include "server/client_connection.hpp"
#include "server/io_service_pool.hpp"
#include "server/timer_master.hpp"
ClientConnection::ClientConnection(
    const string &name, const string &server, const string &port)
    : ProtobufConnection(server + "." + port + "." + name),
      io_service_pool_(new IOServicePool(name + ".IOService", 1, kClientThreadPoolSize)),
      server_(server), port_(port),
      timer_master_(new TimerMaster) {
      VLOG(2) << "Constructor client connection:" << name;
}

ClientConnection::~ClientConnection() {
    CHECK(!IsConnected());
}

bool ClientConnection::Connect() {
  if (IsConnected()) {
    LOG(WARNING) << "Connect but IsConnected";
    return true;
  }
  io_service_pool_->Start();
  timer_master_->Start();
  boost::asio::ip::tcp::resolver::query query(server_, port_);
  boost::asio::ip::tcp::resolver resolver(io_service_pool_->get_io_service());
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator(
      resolver.resolve(query));
  boost::asio::ip::tcp::resolver::iterator end;
  // Try each endpoint until we successfully establish a connection.
  boost::system::error_code error = boost::asio::error::host_not_found;
  boost::asio::ip::tcp::socket *socket =
    new boost::asio::ip::tcp::socket(io_service_pool_->get_io_service());
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
  timer_master_->Register(shared_from_this());
  return true;
}
