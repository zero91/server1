// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include "base/base.hpp"
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include "proto/hello.pb.h"
#include "client/client_connection.hpp"
DEFINE_string(server, "localhost", "The server address");
DEFINE_string(port, "8888", "The server port");
int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  ClientConnection channel(FLAGS_server, FLAGS_port);
  Hello::EchoRequest request;
  Hello::EchoResponse response;
  request.set_question("hello");
  RpcController controller;
  Hello::EchoService::Stub stub(&channel);
  stub.Echo(&controller,
            &request,
            &response,
            NULL);
  LOG(INFO) << response.text();
  return 0;
}
