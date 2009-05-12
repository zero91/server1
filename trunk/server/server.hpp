#ifndef NET2_SERVER_HPP
#define NET2_SERVER_HPP

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include "server/connection.hpp"
#include "server/io_service_pool.hpp"
#include "thread/threadpool.hpp"
// The top-level class of the Server.
class Server
  : private boost::noncopyable, public boost::enable_shared_from_this<Server> {
public:
  /// Construct the Server to listen on the specified TCP address and port, and
  /// serve up files from the given directory.
  explicit Server(int io_service_number,
                  int worker_threads);

  void Listen(const string &address, const string &port,
              ConnectionPtr connection_template);

  /// Stop the Server.
  void Stop();
private:
  // Handle completion of an asynchronous accept operation.
  void HandleAccept(
      const boost::system::error_code& e,
      shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
      shared_ptr<boost::asio::ip::tcp::socket> socket,
      ConnectionPtr connection_template);

  // The pool of io_service objects used to perform asynchronous operations.
  IOServicePool io_service_pool_;
  shared_ptr<ThreadPool> threadpool_;
};
#endif // NET2_SERVER_HPP
