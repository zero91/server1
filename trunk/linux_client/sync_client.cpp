#include "base/base.hpp"
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include "proto/hello.pb.h"
DEFINE_string(server, "localhost", "The server address");
DEFINE_string(port, "8888", "The server port");

DECLARE_bool(logtostderr);
DECLARE_int32(stderrthreshold);

using boost::asio::ip::tcp;

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
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
  CHECK(!error)  << ":fail to connect, error:"  << error.message();

  // Form the request. We specify the "Connection: close" header so that the
  // server will close the socket after transmitting the response. This will
  // allow us to treat all data up until the EOF as the content.
  boost::asio::streambuf request;
  std::ostream request_stream(&request);
  string text;
  int n;
  Hello::LexiconCast a;
  a.set_v1("123456789");
  a.set_v2(10000);
  a.AppendToString(&text);
  string header(lexical_cast<string>(text.length()) + ":");
  n = boost::asio::write(
      socket,
      asio::const_buffers_1(header.c_str(), header.size()),
      asio::transfer_all(), error);
  CHECK(!error) << "write error: " << error.message();
  LOG(INFO) << "n is: " << n;
  n = boost::asio::write(
      socket,
      asio::const_buffers_1(text.c_str(), text.size()),
      asio::transfer_all(), error);
  LOG(INFO) << "n is: " << n;
  CHECK(!error) << "write error: " << error.message();

  // Read the response status line.
  boost::asio::streambuf response;
  // Read until EOF, writing data to output as we go.
  while (boost::asio::read(socket, response,
                           boost::asio::transfer_at_least(1), error))
    std::cout << &response;
  CHECK(error == boost::asio::error::eof) << error.message();
  return 0;
}
