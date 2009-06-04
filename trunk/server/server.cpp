// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include "glog/logging.h"
#include "server/server.hpp"
#include "server/full_dual_channel_proxy.hpp"
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
        Connection *new_connection = connection_template_->Clone();
        server_->HandleAccept(e, socket, new_connection);
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
  const string host(address + "::" + port);
  boost::asio::ip::tcp::acceptor *acceptor = new boost::asio::ip::tcp::acceptor(io_service_pool_.get_io_service());
  boost::asio::ip::tcp::resolver resolver(acceptor->io_service());
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
  connection_template->set_name(host + "::Server");
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
      (*it)->Close();
    }
    notifier_->Dec(1);
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

  for (ChannelTable::iterator it = channel_table_.begin();
       it != channel_table_.end(); ++it) {
    LOG(WARNING) << "leaky channel: " << (*it)->name();
    delete *it;
  }
  channel_table_.clear();
  LOG(WARNING) << "Server stopped";
}

void Server::RemoveConnection(Connection *connection) {
  boost::mutex::scoped_lock locker(channel_table_mutex_);
  channel_table_.erase(connection);
  notifier_->Dec(1);
  VLOG(1) << "Remove connection:" << connection->name();
}

void Server::HandleAccept(const boost::system::error_code& e,
                          boost::asio::ip::tcp::socket *socket,
                          Connection *new_connection) {
  VLOG(2) << "HandleAccept";
  stop_mutex_.lock_shared();
  if (!io_service_pool_.IsRunning()) {
    delete socket;
    delete new_connection;
    stop_mutex_.unlock_shared();
    VLOG(2) << "HandleAccept but already stopped.";
    return;
  }
  stop_mutex_.unlock_shared();
  // The socket ownership transfer to Connection.
  {
    boost::mutex::scoped_lock locker(channel_table_mutex_);
    new_connection->close_signal()->connect(boost::bind(
        &Server::RemoveConnection, this, new_connection));
    new_connection->set_socket(socket);
    channel_table_.insert(new_connection);
    notifier_->Inc(1);
  }
  VLOG(2) << "Insert " << new_connection->name();
  new_connection->ScheduleRead();
}
