#include "glog/logging.h"
#include "server/server.hpp"
#include <boost/bind.hpp>
class AcceptorHandler {
 public:
  AcceptorHandler(boost::asio::ip::tcp::acceptor *acceptor,
                  boost::asio::ip::tcp::socket * socket,
                  const string host,
                  Server *server,
                  Connection *connection_template)
    : acceptor_(acceptor), socket_(socket), host_(host), server_(server), connection_template_(connection_template) {
  }
  void operator()(const boost::system::error_code& e) {
    if (!e) {
      VLOG(2) << "HandleAccept " << host_;
      if (socket_ && socket_->is_open()) {
        VLOG(2) << "Socket is connected";
        boost::asio::io_service &io_service = socket_->get_io_service();
        boost::asio::ip::tcp::socket *live_socket = socket_;
        socket_ = new boost::asio::ip::tcp::socket(io_service);
        Connection *new_connection = connection_template_->Clone();
        new_connection->set_socket(live_socket);
        server_->HandleAccept(e, host_, socket_, new_connection);
      }
      acceptor_->async_accept(*socket_, *this);
    } else {
      VLOG(2) << "HandleAccept error: " << e.message();
      server_->ReleaseAcceptor(host_);
    }
  }
 private:
  string host_;
  // Have the ownership.
  boost::asio::ip::tcp::acceptor *acceptor_;
  boost::asio::ip::tcp::socket * socket_;
  // Haven't the ownership.
  Server *server_;
  Connection *connection_template_;
};

Server::~Server() {
  if (is_running_) {
    Stop();
  }
}

Server::Server(int io_service_number, int worker_threads)
  : io_service_pool_("ServerIOService", io_service_number),
    threadpool_("ServerThreadpool", worker_threads), is_running_(false) {
}

void Server::ReleaseAcceptor(const string &host) {
  if (!is_running_) {
    return;
  }
  boost::mutex::scoped_lock locker(acceptor_table_mutex_);
  AcceptorTable::iterator it = acceptor_table_.find(host);
  if (it == acceptor_table_.end()) {
    VLOG(2) << "Can't find " << host;
    return;
  }
  delete it->second.acceptor;
  delete it->second.socket;
}

void Server::Listen(const string &address,
                    const string &port,
                    Connection* connection_template) {
  is_running_ = true;
  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  VLOG(2) << "Server running";
  threadpool_.Start();
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
  boost::asio::ip::tcp::socket * socket = new boost::asio::ip::tcp::socket(io_service_pool_.get_io_service());
  {
    boost::mutex::scoped_lock locker(acceptor_table_mutex_);
    acceptor_table_.insert(make_pair(host, AcceptorResource(acceptor, socket)));
  }
  connection_template->set_name(host + "::Server");
  acceptor->async_accept(*socket, AcceptorHandler(acceptor, socket, host, this, connection_template));
}

void Server::Stop() {
  if (!is_running_) {
    VLOG(2) << "Server already stopped";
    return;
  }
  is_running_ = false;
  VLOG(2) << "Server stop";
  threadpool_.Stop();
  {
    boost::mutex::scoped_lock locker(connection_table_mutex_);
    for (ConnectionTable::iterator it = connection_table_.begin();
         it != connection_table_.end(); ++it) {
      Connection *connection = *it;
      LOG(WARNING) << "Release connection : " << connection->name();
      connection->Close();
    }
  }
  {
    boost::mutex::scoped_lock locker(acceptor_table_mutex_);
    for (AcceptorTable::iterator it = acceptor_table_.begin(); it != acceptor_table_.end(); ++it) {
      VLOG(2) << "Delete acceptor on " << it->first;
      delete it->second.acceptor;
      delete it->second.socket;
    }
    acceptor_table_.clear();
  }
  io_service_pool_.Stop();
  {
    boost::mutex::scoped_lock locker(connection_table_mutex_);
    for (ConnectionTable::iterator it = connection_table_.begin();
         it != connection_table_.end(); ++it) {
      Connection *connection = *it;
      VLOG(2) << "Delete connection : " << connection->name();
      delete connection;
    }
  }
}

void Server::RemoveConnection(Connection *connection) {
  boost::mutex::scoped_lock locker(connection_table_mutex_);
  connection_table_.erase(connection);
  VLOG(2) << "Remove " << connection->name();
  delete connection;
}

void Server::HandleAccept(const boost::system::error_code& e,
                          const string &host,
                          boost::asio::ip::tcp::socket *socket,
                          Connection *new_connection) {
  VLOG(2) << "HandleAccept";
  {
    boost::mutex::scoped_lock locker(acceptor_table_mutex_);
    AcceptorTable::iterator it = acceptor_table_.find(host);
    CHECK(it != acceptor_table_.end());
    it->second.socket = socket;
  }
  boost::asio::io_service &io_service = io_service_pool_.get_io_service();
  // The socket ownership transfer to Connection.
  new_connection->set_executor(&threadpool_);
  new_connection->set_close_handler(boost::bind(&Server::RemoveConnection, this, new_connection));
  boost::shared_ptr<ConnectionStatus> status(new ConnectionStatus);
  new_connection->set_connection_status(status);
  {
    boost::mutex::scoped_lock locker(connection_table_mutex_);
    connection_table_.insert(new_connection);
    VLOG(2) << "Insert " << new_connection->name();
  }
  new_connection->ScheduleRead();
}
