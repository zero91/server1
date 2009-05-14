#include "client/client_connection.hpp"
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
    LOG(INFO) << "Echo ServiceImpl called";
    response->set_echoed(1);
    response->set_text("server->" + request->question());
    response->set_close(false);
    done->Run();
    connection_ = dynamic_cast<Connection*>(controller);
    VLOG(2) << "connection use count: " << connection_->shared_from_this().use_count() - 1;
    EchoService2::Stub stub(
        dynamic_cast<google::protobuf::RpcChannel*>(controller));
    Hello::EchoRequest2  *request2 = new Hello::EchoRequest2;
    Hello::EchoResponse2 *response2 = new Hello::EchoResponse2;
    request2->set_question("server question" + boost::lexical_cast<string>(i_++));
    RpcController controller2;
    google::protobuf::Closure *done2 = google::protobuf::NewCallback(
        this, &EchoService2ServerImpl::CallEcho2Done, request2, response2);
    stub.Echo2(&controller2,
               request2,
               response2,
               done2);
    VLOG(2) << "connection use count: " << connection_->shared_from_this().use_count() - 1;
    VLOG(2) << "CallEcho2 request: " << request2->question();
  }
  void CallEcho2Done(Hello::EchoRequest2 *request,
                     Hello::EchoResponse2 *response) {
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
  shared_ptr<ProtobufConnection> server_connection;
  shared_ptr<Server> server;
  VLOG(2) << "New server connection";
  server_connection.reset(new ProtobufConnection);
  server.reset(new Server(FLAGS_num_threads, 1));
  EchoService2ServerImpl echo_service;
  server_connection->RegisterService(&echo_service);
  server->Listen(FLAGS_address, FLAGS_port, server_connection);
  sigset_t new_mask;
  sigfillset(&new_mask);
  sigset_t old_mask;
  pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);
  pthread_sigmask(SIG_SETMASK, &old_mask, 0);
  // Wait for signal indicating time to shut down.
  sigset_t wait_mask;
  sigemptyset(&wait_mask);
  sigaddset(&wait_mask, SIGINT);
  sigaddset(&wait_mask, SIGQUIT);
  sigaddset(&wait_mask, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &wait_mask, 0);
  int sig = 0;
  sigwait(&wait_mask, &sig);
  server->Stop();
  return 0;
}
