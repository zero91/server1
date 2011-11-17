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



#ifndef NET2_SERVER_HPP
#define NET2_SERVER_HPP

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <base/hash.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include "server/io_service_pool.hpp"
#include "server/connection.hpp"
#include "thread/notifier.hpp"
#include "server/timer_master.hpp"
class Connection;

class AcceptorHandler;
// The top-level class of the Server.
class Server
  : private boost::noncopyable, public boost::enable_shared_from_this<Server>, public Connection::AsyncCloseListener {
private:
  static const int kDefaultDrainTimeout = LONG_MAX;
public:
  /// Construct the Server to listen on the specified TCP address and port, and
  /// serve up files from the given directory.
  explicit Server(int io_service_number,
                  int worker_threads,
                  int drain_timeout = kDefaultDrainTimeout);
  ~Server();

  void Listen(const string &address, const string &port,
              Connection* connection_template);

  /// Stop the Server.
  void Stop();
private:
  struct AcceptorResource {
    boost::asio::ip::tcp::acceptor *acceptor;
    boost::asio::ip::tcp::socket **socket_pptr;
    AcceptorResource(boost::asio::ip::tcp::acceptor *in_acceptor,
                     boost::asio::ip::tcp::socket **in_socket_pptr)
      : acceptor(in_acceptor), socket_pptr(in_socket_pptr) {
    }
    void Release() {
      acceptor->close();
      delete acceptor;
      (*socket_pptr)->close();
      delete *socket_pptr;
      delete socket_pptr;
    }
  };
  typedef hash_map<string, AcceptorResource> AcceptorTable;
  void ReleaseAcceptor(const string &host);

  void ConnectionClosed(Connection *);
  typedef hash_set<boost::shared_ptr<Connection> > ChannelTable;
  // Handle completion of an asynchronous accept operation.
  void HandleAccept(const boost::system::error_code& e,
                    boost::asio::ip::tcp::socket *socket,
                    Connection *connection_template);
  // The pool of io_service objects used to perform asynchronous operations.
  IOServicePool io_service_pool_;
  TimerMaster timer_master_;
  friend class AcceptorHandler;
  boost::shared_ptr<Notifier> notifier_;
  ChannelTable channel_table_;
  boost::mutex channel_table_mutex_;

  AcceptorTable acceptor_table_;
  boost::mutex acceptor_table_mutex_;

  boost::shared_mutex stop_mutex_;
  int drain_timeout_;
};
#endif // NET2_SERVER_HPP
