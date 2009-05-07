#include "glog/logging.h"
#include "server/server.hpp"
#include <boost/bind.hpp>

Server::Server(const string& address,
               const string& port,
               size_t io_service_pool_size,
               ConnectionPtr connection)
  : io_service_pool_(io_service_pool_size),
    acceptor_(*io_service_pool_.get_io_service().get()),
    connection_(connection) {
  new_connection_.reset(connection_->Clone());
  new_connection_->set_io_service(
      io_service_pool_.get_io_service());
  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  boost::asio::ip::tcp::resolver resolver(acceptor_.io_service());
  boost::asio::ip::tcp::resolver::query query(address, port);
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();
  acceptor_.async_accept(*new_connection_->socket(),
      boost::bind(&Server::HandleAccept, this,
        boost::asio::placeholders::error));
}

void Server::Run() {
  VLOG(2) << "Server running";
  io_service_pool_.Run();
}

void Server::Stop() {
  io_service_pool_.Stop();
}

void Server::HandleAccept(const boost::system::error_code& e) {
  if (!e) {
    VLOG(2) << "Handle accept";
    new_connection_->Start();
    new_connection_.reset(connection_->Clone());
    new_connection_->set_io_service(
      io_service_pool_.get_io_service());
    acceptor_.async_accept(*new_connection_->socket(),
        boost::bind(&Server::HandleAccept, this,
          boost::asio::placeholders::error));
  }
}
