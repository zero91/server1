// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

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
DECLARE_bool(logtostderr);
DECLARE_int32(v);
class EchoService2ServerImpl : public Hello::EchoService2 {
 public:
  EchoService2ServerImpl() : called_(0), i_(0) {
  }
  virtual void Echo1(google::protobuf::RpcController *controller,
                     const Hello::EchoRequest *request,
                     Hello::EchoResponse *response,
                     google::protobuf::Closure *done) {
    VLOG(1) << "CallEcho1 request: " << request->question();
    response->set_echoed(1);
    response->set_text("server->" + request->question());
    response->set_close(false);
    VLOG(1) << "CallEcho1 response: " << response->text();
    string callecho2_question = "server question" + boost::lexical_cast<string>(i_++);
    VLOG(2) << "CallEcho2 tmp request: " << callecho2_question;
    VLOG(2) << "done Run";
    done->Run();
    VLOG(2) << "done Done";
    connection_ = dynamic_cast<Connection*>(controller);
    if (!connection_->IsConnected()) {
      return;
    }
    // CHECK(connection_->IsConnected());
    EchoService2::Stub stub(
        dynamic_cast<google::protobuf::RpcChannel*>(controller));
    Hello::EchoRequest2  *request2 = new Hello::EchoRequest2;
    Hello::EchoResponse2 *response2 = new Hello::EchoResponse2;
    request2->set_question(callecho2_question);
    boost::shared_ptr<RpcController> controller2(new RpcController);
    google::protobuf::Closure *done2 = NewClosure(boost::bind(
        &EchoService2ServerImpl::CallEcho2Done, this, request2, response2, controller2));
    stub.Echo2(controller2.get(),
               request2,
               response2,
               done2);
    VLOG(2) << "CallEcho2 request: " << request2->question();
  }
  void CallEcho2Done(Hello::EchoRequest2 *request,
                     Hello::EchoResponse2 *response,
                     boost::shared_ptr<RpcController> controller) {
    if (!response->text().empty()) {
      CHECK_EQ("client->" + request->question(), response->text());
    }
    ++called_;
    VLOG(2) << "CallEcho2 response:" << response->text();
    delete response;
    delete request;
  }
 private:
  Connection *connection_;
  int called_;
  int i_;
};

int main(int argc, char* argv[]) {
  FLAGS_v = 4;
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  sigset_t mask;
  sigfillset(&mask); /* Mask all allowed signals */
  int rc = pthread_sigmask(SIG_SETMASK, &mask, NULL);
  VLOG(2) << "Signal masked" << rc;
  boost::shared_ptr<ProtobufConnection> server_connection;
  boost::shared_ptr<Server> server;
  VLOG(2) << "New server connection";
  server_connection.reset(new ProtobufConnection("Server"));
  server.reset(new Server(1, FLAGS_num_threads));
  EchoService2ServerImpl echo_service;
  server_connection->RegisterService(&echo_service);
  server->Listen(FLAGS_address, FLAGS_port, server_connection.get());
  sigset_t new_mask;
  sigfillset(&new_mask);
  sigset_t old_mask;
  sigset_t wait_mask;
  sigemptyset(&wait_mask);
  sigaddset(&wait_mask, SIGINT);
  sigaddset(&wait_mask, SIGQUIT);
  sigaddset(&wait_mask, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);
  pthread_sigmask(SIG_SETMASK, &old_mask, 0);
  // Wait for signal indicating time to shut down.
  pthread_sigmask(SIG_BLOCK, &wait_mask, 0);
  int sig = 0;
  sigwait(&wait_mask, &sig);
  VLOG(2) << "Catch signal" << sig;
  server->Stop();
  return 0;
}
