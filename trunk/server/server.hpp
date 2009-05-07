#ifndef NET2_SERVER_HPP
#define NET2_SERVER_HPP

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include "server/connection.hpp"
#include "server/io_service_pool.hpp"
// The top-level class of the Server.
class Server
  : private boost::noncopyable {
public:
  /// Construct the Server to listen on the specified TCP address and port, and
  /// serve up files from the given directory.
  explicit Server(
      const string& address,
      const string& port,
      size_t io_service_pool_size,
      ConnectionPtr connection);

  /// Run the Server's io_service loop.
  void Run();

  /// Stop the Server.
  void Stop();

private:
  /// Handle completion of an asynchronous accept operation.
  void HandleAccept(const boost::system::error_code& e);

  /// The pool of io_service objects used to perform asynchronous operations.
  IOServicePool io_service_pool_;

  /// Acceptor used to listen for incoming connections.
  boost::asio::ip::tcp::acceptor acceptor_;

  /// The next connection to be accepted.
  ConnectionPtr new_connection_;
  // The original connection with the handler table.
  ConnectionPtr connection_;
};
#endif // NET2_SERVER_HPP
