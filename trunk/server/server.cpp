#include "glog/logging.h"
#include "server/server.hpp"
#include <boost/bind.hpp>

Server::Server(int io_service_number, int worker_threads)
  : io_service_pool_(io_service_number),
    threadpool_(new ThreadPool(worker_threads)) {
}

void Server::Listen(const string &address,
                    const string &port,
                    ConnectionPtr connection_template) {
  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  VLOG(2) << "Server running";
  threadpool_->Start();
  io_service_pool_.Start();
  shared_ptr<boost::asio::ip::tcp::acceptor> acceptor(new boost::asio::ip::tcp::acceptor(
      *io_service_pool_.get_io_service().get()));
  boost::asio::ip::tcp::resolver resolver(acceptor->io_service());
  boost::asio::ip::tcp::resolver::query query(address, port);
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
  acceptor->open(endpoint.protocol());
  acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor->bind(endpoint);
  acceptor->listen();
  shared_ptr<boost::asio::ip::tcp::socket> socket(
      new boost::asio::ip::tcp::socket(*io_service_pool_.get_io_service().get()));
  acceptor->async_accept(*socket.get(),
      boost::bind(&Server::HandleAccept, shared_from_this(),
        boost::asio::placeholders::error, acceptor, socket, connection_template));
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
      ConnectionPtr new_connection(connection_template->Clone());
      new_connection->set_socket(socket);
      new_connection->set_executor(
          threadpool_->shared_from_this());
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
