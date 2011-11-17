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
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "proto/hello.pb.h"
#include "boost/thread.hpp"
#include "thread/pcqueue.hpp"
#include <sstream>

DEFINE_string(server, "localhost", "The test server");
DEFINE_string(port, "6789", "The test server");
DEFINE_int32(num_threads, 4, "The test server thread number");
DEFINE_int32(num_connections, 4, "The test server thread number");
DECLARE_bool(logtostderr);
DECLARE_int32(v);

class EchoService2Impl : public Hello::EchoService2 {
 public:
 public:
  EchoService2Impl(boost::shared_ptr<PCQueue<bool> > pcqueue) : called_(0), cnt_(0), pcqueue_(pcqueue) {
  }
  virtual void Echo1(google::protobuf::RpcController *controller,
                    const Hello::EchoRequest *request,
                    Hello::EchoResponse *response,
                    google::protobuf::Closure *done) {
    LOG(INFO) << "Echo ServiceImpl called";
    response->set_echoed(1);
    response->set_text("server->" + request->question());
    response->set_close(false);
    done->Run();
    EchoService2::Stub stub(
        dynamic_cast<google::protobuf::RpcChannel*>(controller));
    Hello::EchoRequest2  *request2 = new Hello::EchoRequest2;
    Hello::EchoResponse2 *response2 = new Hello::EchoResponse2;
    request2->set_question("server question" + boost::lexical_cast<string>(i_++));
    boost::shared_ptr<RpcController> controller2(new RpcController);
    google::protobuf::Closure *done2 = NewClosure(boost::bind(
        &EchoService2Impl::CallEcho2Done, this, request2, response2, controller2));
    stub.Echo2(controller2.get(),
               request2,
               response2,
               done2);
    connection_ = dynamic_cast<Connection*>(controller);
    VLOG(2) << "CallEcho2 request: " << request2->question();
  }
  virtual void Echo2(google::protobuf::RpcController *controller,
                     const Hello::EchoRequest2 *request,
                    Hello::EchoResponse2 *response,
                    google::protobuf::Closure *done) {
    response->set_echoed(2);
    response->set_text("client->" + request->question());
    response->set_close(false);
    LOG(INFO) << "Echo2 called response with:" << response->text();
    done->Run();
  }
  void CallEcho2Done(Hello::EchoRequest2 *request,
                     Hello::EchoResponse2 *response,
                     boost::shared_ptr<RpcController> controller) {
    static int cnt = 0;
    if (!response->text().empty()) {
      CHECK_EQ("client->" + request->question(), response->text());
    }
    ++called_;
    LOG(INFO) << "CallEcho2 response:" << response->text();
    delete response;
    delete request;
    pcqueue_->Push(true);
    LOG(INFO) << "PCQueue push: " <<  called_;
  }
  int called() const {
    return called_;
  }
 private:
  boost::shared_ptr<PCQueue<bool> > pcqueue_;
  Connection *connection_;
  int cnt_;
  int called_;
  int i_;
};

class ListenTest : public testing::Test {
 public:
  ListenTest() {
  }

  void SetUp() {
    VLOG(2) << "New server connection";
    server_connection_.reset(new ProtobufConnection("ListenTest.Server"));
    server_.reset(new Server(2, FLAGS_num_threads));
    VLOG(2) << "New client connection";
    client_connection_.reset(new ClientConnection("ListenTestMainClient", FLAGS_server, FLAGS_port));
    client_stub_.reset(new Hello::EchoService2::Stub(client_connection_.get()));
    pcqueue_.reset(new PCQueue<bool>);
    echo_service_.reset(new EchoService2Impl(pcqueue_));
    VLOG(2) << "Register service";
    server_connection_->RegisterService(echo_service_.get());
    client_connection_->RegisterService(echo_service_.get());
    server_->Listen(FLAGS_server, FLAGS_port, server_connection_.get());
    CHECK(!client_connection_->IsConnected());
    CHECK(client_connection_->Connect());
    aborted_ = 0;
    called_ = 0;
  }

  void TearDown() {
    VLOG(2) << "Reset server connection";
    server_connection_.reset();
    server_->Stop();
    server_.reset();
    client_stub_.reset();
    VLOG(2) << "Reset client connection";
    client_connection_->Disconnect();
    client_connection_.reset();
    pcqueue_.reset();
    echo_service_.reset();
  }

  void ClientCallDone(boost::shared_ptr<Hello::EchoResponse> response, boost::shared_ptr<RpcController> controller) {
    VLOG(2) << "ClientCallDone";
  }

  void ClientCallMultiThreadDone(
      boost::shared_ptr<RpcController> controller,
      boost::shared_ptr<Hello::EchoRequest> request,
      boost::shared_ptr<Hello::EchoResponse> response) {
    VLOG(2) << "CallEcho response: " << response->text();
    VLOG(2) << "ClientCallMultiThreadDone called";
    if (!controller->Failed()) {
      CHECK_EQ("server->" + request->question(), response->text());
    } else {
//      CHECK_EQ(controller->ErrorText(), "Abort");
      pcqueue_->Push(true);
      ++aborted_;
      VLOG(0) << "abord: " << aborted_ << " called: " << called_;
    }
    ++called_;
  }

  void ClientThreadRun(boost::shared_ptr<Hello::EchoService2::Stub> client_stub) {
    static int i = 0;
    boost::shared_ptr<Hello::EchoRequest> request(new Hello::EchoRequest);
    boost::shared_ptr<Hello::EchoResponse> response(new Hello::EchoResponse);
    request->set_question("client question" + boost::lexical_cast<string>(i++));
    boost::shared_ptr<RpcController> controller(new RpcController);
    client_stub->Echo1(controller.get(),
                       request.get(),
                       response.get(),
                       NewClosure(boost::bind(
                           &ListenTest::ClientCallMultiThreadDone,
                           this, controller, request, response)));
    VLOG(2) << "CallEcho request: " << request->question();
  }
 protected:
  boost::shared_ptr<ProtobufConnection> server_connection_;
  boost::shared_ptr<Server> server_;
  boost::shared_ptr<ClientConnection> client_connection_;
  RpcController listener_controller_;
  boost::shared_ptr<Hello::EchoService2::Stub> client_stub_;
  boost::shared_ptr<PCQueue<bool> > pcqueue_;
  boost::scoped_ptr<EchoService2Impl> echo_service_;
  int aborted_;
  int called_;
};

TEST_F(ListenTest, Test1) {
  Hello::EchoRequest request;
  boost::shared_ptr<Hello::EchoResponse> response(new Hello::EchoResponse);
  request.set_question("client question");
  boost::shared_ptr<RpcController> controller(new RpcController);
  client_stub_->Echo1(controller.get(),
                      &request,
                      response.get(),
                      NewClosure(boost::bind(
                          &ListenTest::ClientCallDone, this, response, controller)));
  pcqueue_->Pop();
  VLOG(2) << "Poped!!";
  client_connection_->Disconnect();
  CHECK_EQ("server->" + request.question(), response->text());
  CHECK_EQ(echo_service_->called(), 1);
}

TEST_F(ListenTest, MultiThreadTest1) {
  CHECK(client_connection_->Connect());
  // Create a pool of threads to run all of the io_services.
  vector<boost::shared_ptr<boost::thread> > threads;
  for (size_t i = 0; i < FLAGS_num_threads; ++i) {
    boost::thread t(boost::thread(&ListenTest::ClientThreadRun, this, client_stub_));
  }
  int cnt = 0;
  while (pcqueue_->Pop()) {
    ++cnt;
    VLOG(2) << "Get on pop" << cnt;
    if (cnt == FLAGS_num_threads) {
      break;
    }
  }
  client_connection_->Disconnect();
  VLOG(2) << "Close client connection";
  CHECK_EQ(aborted_ + echo_service_->called(), FLAGS_num_threads);
}

TEST_F(ListenTest, MultiThreadMultConnectionTest1) {
  ThreadPool pool("Test", FLAGS_num_threads);
  pool.Start();
  boost::shared_ptr<IOServicePool> client_io(new IOServicePool("TestIO", 2, 4));
  boost::shared_ptr<TimerMaster> tm(new TimerMaster);
  client_io->Start();
  tm->Start();
  vector<boost::shared_ptr<ClientConnection> > connections;
  vector<boost::shared_ptr<Hello::EchoService2::Stub> > stubs;
  for (int i = 0; i < FLAGS_num_connections; ++i) {
    string name("MultiThreadMultConnectionTest1Client." + boost::lexical_cast<string>(i));
    boost::shared_ptr<ClientConnection> r(new ClientConnection(name, FLAGS_server, FLAGS_port));
    r->RegisterService(echo_service_.get());
    r->set_io_service_pool(client_io);
    r->set_timer_master(tm);
    connections.push_back(r);
    CHECK(r->Connect());
    boost::shared_ptr<Hello::EchoService2::Stub> s(new Hello::EchoService2::Stub(connections.back().get()));
    stubs.push_back(s);
    for (int j = 0; j < FLAGS_num_threads; ++j) {
      pool.PushTask(boost::bind(&ListenTest::ClientThreadRun, this, stubs.back()));
    }
  }
  int cnt = 0;
  while (pcqueue_->Pop()) {
    LOG(INFO) << "Get on pop" << cnt++;
    if (cnt == FLAGS_num_threads * FLAGS_num_connections) {
      break;
    }
  }
  for (int i = 0; i < FLAGS_num_connections; ++i) {
    connections[i]->Disconnect();
  }
  pool.Stop();
  client_io->Stop();
  tm->Stop();
  VLOG(2) << "Close client connection";
}

int main(int argc, char **argv) {
  FLAGS_v = 4;
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
