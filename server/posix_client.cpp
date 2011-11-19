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
#include "server/protobuf_connection.hpp"
#include <iostream>
#include <string>
#include <pthread.h>
#include <signal.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <boost/thread.hpp>
#include "proto/hello.pb.h"
DEFINE_string(address, "localhost","The address");
DEFINE_string(port, "6789","The port");
DEFINE_int32(num_threads, 4,"The thread size");
DEFINE_int32(num_connections, 4, "The test server thread number");
//DECLARE_bool(logtostderr);
//DECLARE_int32(v);
class EchoService2ClientImpl : public Hello::EchoService2 {
 public:
  EchoService2ClientImpl(boost::shared_ptr<PCQueue<bool> > pcqueue) : called_(0), pcqueue_(pcqueue) {
  }
  virtual void Echo2(google::protobuf::RpcController *controller,
                     const Hello::EchoRequest2 *request,
                    Hello::EchoResponse2 *response,
                    google::protobuf::Closure *done) {
    response->set_echoed(2);
    response->set_text("client->" + request->question());
    response->set_close(false);
    VLOG(1) << "Echo2 called response with:" << response->text();
    done->Run();
    ++called_;
    VLOG(2) << "CallEcho2Done response:" << response->text() << called_;
    pcqueue_->Push(true);
  }
  int called() const {
    return called_;
  }
 private:
  Connection *connection_;
  int called_;
  boost::shared_ptr<PCQueue<bool> > pcqueue_;
};

void ClientCallMultiThreadDone(
    boost::shared_ptr<RpcController> controller,
    boost::shared_ptr<Hello::EchoRequest> request,
    boost::shared_ptr<Hello::EchoResponse> response) {
  VLOG(2) << "ClientCallMultiThreadDone called";
  CHECK_EQ("server->" + request->question(), response->text());
  VLOG(2) << "CallEcho1Response: " << response->text();
}

void ClientThreadRun(boost::shared_ptr<Hello::EchoService2::Stub> client_stub, int i) {
  boost::shared_ptr<Hello::EchoRequest> request(new Hello::EchoRequest);
  boost::shared_ptr<Hello::EchoResponse> response(new Hello::EchoResponse);
  request->set_question("client question" + boost::lexical_cast<string>(i));
  boost::shared_ptr<RpcController> controller(new RpcController);
  VLOG(2) << "CallEcho1Request: " << request->question();
  client_stub->Echo1(controller.get(),
                      request.get(),
                      response.get(),
                      NewClosure(boost::bind(
                          &ClientCallMultiThreadDone,
                          controller, request, response)));
}

int main(int argc, char* argv[]) {
  FLAGS_v = 4;
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  boost::shared_ptr<PCQueue<bool> > pcqueue(new PCQueue<bool>);
  boost::scoped_ptr<EchoService2ClientImpl> echo_service;
  echo_service.reset(new EchoService2ClientImpl(pcqueue));
  ThreadPool pool("PosixClient", FLAGS_num_threads);
  pool.Start();
  vector<boost::shared_ptr<ClientConnection> > connections;
  vector<boost::shared_ptr<Hello::EchoService2::Stub> > stubs;
  boost::shared_ptr<IOServicePool> client_io(new IOServicePool("PosixClientIO", 2, 4));
  client_io->Start();
  for (int i = 0; i < FLAGS_num_connections; ++i) {
    string name("PosixClient." + boost::lexical_cast<string>(i));
    boost::shared_ptr<ClientConnection> r(new ClientConnection(name, FLAGS_address, FLAGS_port));
    r->RegisterService(echo_service.get());
    r->set_io_service_pool(client_io);
    connections.push_back(r);
    while (1) {
      try {
        if (r->Connect()) {
          VLOG(1) << "Connect " << i;
          break;
        }
      } catch (std::exception e) {
        VLOG(0) << "exception: " << e.what();
      }
      sleep(1);
      VLOG(0) << "Retry " << i;
    }
    boost::shared_ptr<Hello::EchoService2::Stub> s(new Hello::EchoService2::Stub(connections.back().get()));
    stubs.push_back(s);
  }
  VLOG(0) << "All connected";
  for (int i = 0; i < FLAGS_num_connections; ++i) {
    for (int j = 0; j < FLAGS_num_threads; ++j) {
      pool.PushTask(boost::bind(ClientThreadRun, stubs.back(), i * FLAGS_num_connections + j));
    }
  }
  int cnt = 0;
  while (1) {
    pcqueue->Pop();
    ++cnt;
    VLOG(1) << "Get on pop";
    if (cnt == FLAGS_num_threads * FLAGS_num_connections) {
      break;
    }
  }
  VLOG(0) << "Disconnect";
  for (int i = 0; i < FLAGS_num_connections; ++i) {
    VLOG(1) << "Disconnect " << i;
    connections[i]->Disconnect();
  }
  VLOG(0) << "Stop thread pool";
  pool.Stop();
  VLOG(0) << "Stop io service";
  client_io->Stop();
  return 0;
}
