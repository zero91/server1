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



#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP
#include <boost/thread/shared_mutex.hpp>
#include "server/protobuf_connection.hpp"
class TimerMaster;
class IOServicePool;
class ClientConnection : public ProtobufConnection {
 public:
  ClientConnection(const string &name, const string &server, const string &port);
  bool Connect();

  void set_io_service_pool(
      boost::shared_ptr<IOServicePool> io_service_pool) {
    io_service_pool_ = io_service_pool;
  }
  void set_timer_master(
      boost::shared_ptr<TimerMaster> timer_master) {
    timer_master_ = timer_master;
  }
  ~ClientConnection();
 private:
  static const int kClientThreadPoolSize = 1;
  boost::shared_ptr<IOServicePool> io_service_pool_;
  boost::shared_ptr<TimerMaster> timer_master_;
  string server_, port_;
};
#endif  // CLIENT_CONNECTION_HPP
