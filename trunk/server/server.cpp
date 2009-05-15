#include "glog/logging.h"
#include "server/server.hpp"
#include <boost/bind.hpp>
struct ConenctionResource {
  Connection *connection;
  boost::asio::ip::tcp::socket *socket;
};

static void RemoveConnection(ConnectionResource * resource) {
  VLOG(2) << resource->connection->name() << " removed";
  delete resource->socket;
  delete resource->connection;
  delete resource;
};

struct AcceptorResource {
  string host;
  // Have the ownership.
  boost::asio::ip::tcp::acceptor *acceptor;
  boost::asio::ip::tcp::socket * socket;
  // Haven't the ownership.
  const Connection *connection_template;
  void operator()(const boost::system::error_code& e) {
    if (!e) {
      VLOG(2) << "HandleAccept " << host;
      if (socket && socket->is_open()) {
        boost::asio::io_service &io_service = socket->get_io_service();
        VLOG(2) << "Socket is connected";
        ConnectionResource resource = new ConnectionResource;
        Connection *new_connection = connection_template->Clone();
        new_connection->set_socket(socket);
        new_connection->set_executor(threadpool_);
        new_connection->set_allocator(&allocator_);
        resource->connection = new_connection;
        resource->socket = socket;
        new_connection->set_close_handler(boost::bind(
            &RemoveConnection, resource));
        new_connection->ScheduleRead();
        socket = new boost::asio::ip::tcp::socket(io_service);
      }
      acceptor->async_accept(*socket, *this);
    } else {
      VLOG(2) << "HandleAccept error: " << e.message();
      delete acceptor;
      delete socket;
    }
  }
};

Server::Server(int io_service_number, int worker_threads)
  : io_service_pool_(io_service_number),
    threadpool_(worker_threads) {
}

void Server::Listen(const string &address,
                    const string &port,
                    ConnectionPtr connection_template) {
  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  VLOG(2) << "Server running";
  threadpool_->Start();
  io_service_pool_.Start();
  const string host(address + "::" + port);
  AcceptorResource *acceptor_resource = NULL;
  {
    boost::mutex::scoped_lock locker(acceptor_table_mutex_);
    AcceptorTable::const_iterator it = acceptor_table_.find(host);
    if (it != acceptor_table_.end()) {
      LOG(WARNING) << "Already listen on " << host;
      return;
    }
    acceptor_resource = &acceptor_table_[host];
  }

  CHECK(acceptor_resource);
  boost::asio::ip::tcp::acceptor *acceptor = new boost::asio::ip::tcp::acceptor(io_service_pool_.get_io_service());
  boost::asio::ip::tcp::resolver resolver(acceptor->io_service());
  boost::asio::ip::tcp::resolver::query query(address, port);
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
  acceptor->open(endpoint.protocol());
  acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor->bind(endpoint);
  acceptor->listen();
  boost::asio::ip::tcp::socket * socket = new boost::asio::ip::tcp::socket(io_service_pool_.get_io_service());
  acceptor->async_accept(*socket, boost::bind(&Server::HandleAccept, this, boost::asio::placeholders::error));
  acceptor_resource->acceptor = acceptor;
  acceptor_resource->
}

void Server::Stop() {
  VLOG(2) << "Server stop";
  threadpool_->Stop();
  io_service_pool_.Stop();
}

void Server::HandleAccept(const boost::system::error_code& e,
                          shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
                          shared_ptr<boost::asio::ip::tcp::socket> socket,
                          ConnectionPtr connection_template) {
  if (!e) {
    VLOG(2) << "HandleAccept";
    if (socket.get() && socket->is_open()) {
      VLOG(2) << "Socket is connected";
      shared_ptr<ConnectionResource> resource(new ConnectionResource);
      Connection *new_connection = connection_template->Clone();
      new_connection->set_socket(socket.get());
      new_connection->set_executor(threadpool_);
      new_connection->set_allocator(&allocator_);
      resource->connection = new_connection;
      resource->socket = socket;
      new_connection->set_close_handler(boost::bind(
          &RemoveConnection, new_connection, socket));
      new_connection->ScheduleRead();
    }
    shared_ptr<boost::asio::ip::tcp::socket> new_socket(
        new boost::asio::ip::tcp::socket(*io_service_pool_.get_io_service()));
    acceptor->async_accept(*new_socket.get(),
        boost::bind(&Server::HandleAccept, shared_from_this(),
          boost::asio::placeholders::error, acceptor, new_socket, connection_template));
  } else {
    VLOG(2) << "HandleAccept error: " << e.message();
  }
}
