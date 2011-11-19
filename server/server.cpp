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



#include "glog/logging.h"
#include "server/server.hpp"
#include "server/protobuf_connection.hpp"
#include <boost/bind.hpp>
class AcceptorHandler {
 public:
  AcceptorHandler(boost::asio::ip::tcp::acceptor *acceptor,
                  boost::asio::ip::tcp::socket **socket_pptr,
                  const string host,
                  Server *server,
                  Connection *connection_template)
    : acceptor_(acceptor), socket_pptr_(socket_pptr), host_(host), server_(server), connection_template_(connection_template) {
  }
  void operator()(const boost::system::error_code& e) {
    if (!e) {
      VLOG(2) << "HandleAccept " << host_;
      boost::asio::ip::tcp::socket *socket = *socket_pptr_;
      if (socket && socket->is_open()) {
        VLOG(2) << "Socket is connected";
        boost::asio::io_service &io_service = socket->get_io_service();
        *socket_pptr_ = new boost::asio::ip::tcp::socket(io_service);
        acceptor_->async_accept(**socket_pptr_, *this);
        server_->HandleAccept(e, socket, connection_template_);
      } else {
        acceptor_->async_accept(**socket_pptr_, *this);
      }
    } else {
      VLOG(1) << "HandleAccept error: " << e.message();
      server_->ReleaseAcceptor(host_);
    }
  }
 private:
  string host_;
  // Have the ownership.
  boost::asio::ip::tcp::acceptor *acceptor_;
  boost::asio::ip::tcp::socket **socket_pptr_;
  // Haven't the ownership.
  Server *server_;
  Connection *connection_template_;
};

Server::~Server() {
  if (io_service_pool_.IsRunning()) {
    Stop();
  }
}

Server::Server(int io_service_number,
               int worker_threads,
               int drain_timeout)
  : io_service_pool_("ServerIOService",
                     io_service_number, worker_threads),
    notifier_(new Notifier("ServerNotifier", 1)),
    drain_timeout_(drain_timeout) {
}

void Server::ReleaseAcceptor(const string &host) {
  boost::mutex::scoped_lock locker(acceptor_table_mutex_);
  AcceptorTable::iterator it = acceptor_table_.find(host);
  if (it == acceptor_table_.end()) {
    VLOG(2) << "Can't find " << host;
    return;
  }
  it->second.Release();
}

void Server::Listen(const string &address,
                    const string &port,
                    Connection* connection_template) {
  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  VLOG(2) << "Server running";
  io_service_pool_.Start();
  timer_master_.Start();
  const string host(address + "::" + port);
  boost::asio::ip::tcp::acceptor *acceptor = new boost::asio::ip::tcp::acceptor(io_service_pool_.get_io_service());
  boost::asio::ip::tcp::resolver resolver(acceptor->get_io_service());
  boost::asio::ip::tcp::resolver::query query(address, port);
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
  acceptor->open(endpoint.protocol());
  acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor->bind(endpoint);
  acceptor->listen();
  boost::asio::ip::tcp::socket **socket_pptr = new (boost::asio::ip::tcp::socket*);
  *socket_pptr = new boost::asio::ip::tcp::socket(io_service_pool_.get_io_service());
  {
    boost::mutex::scoped_lock locker(acceptor_table_mutex_);
    acceptor_table_.insert(make_pair(host, AcceptorResource(acceptor, socket_pptr)));
  }
  acceptor->async_accept(**socket_pptr, AcceptorHandler(acceptor, socket_pptr, host, this, connection_template));
}

void Server::Stop() {
  VLOG(2) << "Server stop";
  stop_mutex_.lock();
  if (!io_service_pool_.IsRunning()) {
    VLOG(2) << "Server already stopped";
    stop_mutex_.unlock();
    return;
  }
  {
    boost::mutex::scoped_lock locker(channel_table_mutex_);
    for (ChannelTable::iterator it = channel_table_.begin();
         it != channel_table_.end(); ++it) {
      VLOG(2) << "Close: " << (*it)->name();
      (*it)->Disconnect();
      notifier_->Dec(1);
    }
    notifier_->Dec(1);
    channel_table_.clear();
  }
  // All channel had been flushed.
  notifier_->Wait();
  {
    boost::mutex::scoped_lock locker(acceptor_table_mutex_);
    for (AcceptorTable::iterator it = acceptor_table_.begin(); it != acceptor_table_.end(); ++it) {
      VLOG(2) << "Delete acceptor on " << it->first;
      it->second.Release();
    }
    acceptor_table_.clear();
  }
  LOG(WARNING) << "Stop io service pool";
  io_service_pool_.Stop();
  stop_mutex_.unlock();
  timer_master_.Stop();
  LOG(WARNING) << "Server stopped";
  CHECK(channel_table_.empty());
}

void Server::ConnectionClosed(Connection *connection) {
  boost::mutex::scoped_lock locker(channel_table_mutex_);
  int size1 = channel_table_.size();
  boost::shared_ptr<Connection> conn = connection->shared_from_this();
  VLOG(2) << "ConnectionClosed: " << conn.get();
  if (channel_table_.find(conn) != channel_table_.end()) {
    channel_table_.erase(conn);
    notifier_->Dec(1);
    VLOG(1) << "Remove connection:" << connection->name();
  }  else {
    VLOG(1) << "Had removed connection:" << connection->name();
  }
}

void Server::HandleAccept(const boost::system::error_code& e,
                          boost::asio::ip::tcp::socket *socket,
                          Connection *connection_template) {
  VLOG(2) << "HandleAccept";
  stop_mutex_.lock_shared();
  if (!io_service_pool_.IsRunning()) {
    delete socket;
    stop_mutex_.unlock_shared();
    VLOG(2) << "HandleAccept but already stopped.";
    return;
  }
  // The socket ownership transfer to Connection.
  boost::shared_ptr<Connection> connection = connection_template->Span(
      socket);
  if (connection.get() == NULL) {
    LOG(WARNING) << "Span a NULL connection!";
    return;
  }
  {
    boost::mutex::scoped_lock locker(channel_table_mutex_);
    boost::shared_ptr<Server> s = this->shared_from_this();
    if (!connection->RegisterAsyncCloseListener(s)) {
      LOG(WARNING) << "RegisterAsyncCloseListener failed";
      connection->Disconnect();
      return;
    }
    channel_table_.insert(connection);
    timer_master_.Register(connection);
    VLOG(2) << "Insert: " << connection.get();
    notifier_->Inc(1);
  }
  VLOG(2) << "Insert " << connection->name();
  stop_mutex_.unlock_shared();
}
