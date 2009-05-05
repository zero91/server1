#include "base/base.hpp"
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include "gflags/gflags.h"
#include "glog/logging.h"
DEFINE_string(server, "localhost", "The server address");
DEFINE_string(port, "8888", "The server port");

using boost::asio::ip::tcp;

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver::query query(
      FLAGS_server, FLAGS_port);
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
  tcp::resolver::iterator end;

  // Try each endpoint until we successfully establish a connection.
  tcp::socket socket(io_service);
  boost::system::error_code error = boost::asio::error::host_not_found;
  while (error && endpoint_iterator != end) {
    socket.close();
    socket.connect(*endpoint_iterator++, error);
  }
  if (error) {
    throw boost::system::system_error(error);
  }

  // Form the request. We specify the "Connection: close" header so that the
  // server will close the socket after transmitting the response. This will
  // allow us to treat all data up until the EOF as the content.
  boost::asio::streambuf request;
  std::ostream request_stream(&request);
  string text = "hello world\n";
  request_stream << lexical_cast<string>(text.length())
    << ":" << text;
  LOG(INFO) << "content:" << &request;
  boost::asio::write(socket, request);

  // Read the response status line.
  boost::asio::streambuf response;
  // Read until EOF, writing data to output as we go.
  while (boost::asio::read(socket, response,
                           boost::asio::transfer_at_least(1), error))
    std::cout << &response;
  if (error != boost::asio::error::eof)
    throw boost::system::system_error(error);
  return 0;
}
