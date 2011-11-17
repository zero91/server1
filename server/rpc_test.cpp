/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */



#include "server/client_connection.hpp"
#include "server/server.hpp"
#include <gtest/gtest.h>
#include "proto/hello.pb.h"
#include "boost/thread.hpp"

DEFINE_string(server, "localhost", "The test server");
DEFINE_string(port, "6789", "The test server");
DEFINE_int32(num_threads, 1, "The test server thread number");
DECLARE_bool(logtostderr);
DECLARE_int32(v);


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
    server_connection_.reset(new ProtobufConnection("ServerConnection"));
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
    server_->Stop();
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
  boost::shared_ptr<ProtobufConnection> server_connection_;
  boost::shared_ptr<ClientConnection> client_connection_;
  boost::shared_ptr<Server> server_;
  EchoServiceImpl echo_service_;
  boost::scoped_ptr<Hello::EchoService::Stub> stub_;
  boost::scoped_ptr<google::protobuf::Closure> done_;
};

TEST_F(EchoTest, Test1) {
  Hello::EchoRequest request;
  Hello::EchoResponse response;
  request.set_question("hello");
  RpcController controller;
  CHECK(client_connection_->IsConnected());
  stub_->Echo(&controller,
              &request,
              &response,
              done_.get());
  controller.Wait();
  VLOG(2) << "client service return";
  LOG(INFO) << response.text();
  EXPECT_EQ(request.question(), response.text());
  EXPECT_FALSE(controller.Failed()) << controller.ErrorText();
  client_connection_->Disconnect();
}

int main(int argc, char **argv) {
  FLAGS_v = 4;
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
