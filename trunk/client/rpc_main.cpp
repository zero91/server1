#include "base/base.hpp"
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include "proto/hello.pb.h"
#include "client/channel.hpp"
DEFINE_string(server, "localhost", "The server address");
DEFINE_string(port, "8888", "The server port");
int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
  RpcChannel channel(io_service, FLAGS_server, FLAGS_port);
  Hello::EchoRequest request;
  Hello::EchoResponse response;
  request.set_question("hello");
  RpcController controller;
  Hello::EchoService::Stub stub(&channel);
  stub.Echo(&controller,
              &request,
              &response,
              NULL);
  io_service->run();
  LOG(INFO) << response.text();
  return 0;
}
