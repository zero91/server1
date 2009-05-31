// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include "client/client_connection.hpp"
#include "server/server.hpp"
#include <gtest/gtest.h>
#include "proto/hello.pb.h"
#include "boost/thread.hpp"

DEFINE_string(server, "localhost", "The test server");
DEFINE_string(port, "6789", "The test server");
DEFINE_int32(num_threads, 1, "The test server thread number");


class EchoServiceImpl : public Hello::EchoService {
 public:
  EchoServiceImpl() {
  }

  virtual void Echo(::google::protobuf::RpcController*,
                    const Hello::EchoRequest *request,
                    Hello::EchoResponse *response,
                    google::protobuf::Closure *done) {
    LOG(INFO) << "Echo ServiceImpl called";
    response->set_echoed(true);
    response->set_text(request->question());
    response->set_close(true);
    done->Run();
  }
};


class EchoTest : public testing::Test {
 public:
  EchoTest() {
  }

  void SetUp() {
    VLOG(2) << "New server connection";
    server_connection_.reset(new ProtobufConnection);
    server_.reset(new Server(FLAGS_num_threads, 1));
    VLOG(2) << "New client connection";
    client_connection_.reset(new ClientConnection(
        "EchoTestMainClient",
        FLAGS_server, FLAGS_port));
    stub_.reset(new Hello::EchoService::Stub(client_connection_.get()));
    server_->Listen(FLAGS_server, FLAGS_port, server_connection_.get());
    VLOG(2) << "Register service";
    server_connection_->RegisterService(&echo_service_);
    CHECK(!client_connection_->IsConnected());
    CHECK(client_connection_->Connect());
    done_.reset(google::protobuf::NewPermanentCallback(
      this, &EchoTest::CallDone));
  }

  void TearDown() {
    VLOG(2) << "Reset server connection";
    server_connection_.reset();
    server_.reset();
    VLOG(2) << "Reset client connection";
    client_connection_.reset();
    stub_.reset();
    done_.reset();
  }

  void CallDone() {
    LOG(INFO) << "Call done is called";
  }
 protected:
  boost::scoped_ptr<ProtobufConnection> server_connection_;
  boost::scoped_ptr<ClientConnection> client_connection_;
  boost::scoped_ptr<Server> server_;
  EchoServiceImpl echo_service_;
  boost::scoped_ptr<Hello::EchoService::Stub> stub_;
  boost::scoped_ptr<google::protobuf::Closure> done_;
};

TEST_F(EchoTest, Test1) {
  Hello::EchoRequest request;
  Hello::EchoResponse response;
  request.set_question("hello");
  RpcController controller;
  stub_->Echo(&controller,
              &request,
              &response,
              done_.get());
  VLOG(2) << "client service return";
  LOG(INFO) << response.text();
  EXPECT_EQ(request.question(), response.text());
  EXPECT_FALSE(controller.Failed()) << controller.ErrorText();
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
