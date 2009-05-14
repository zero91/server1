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
class EchoService2ClientImpl : public Hello::EchoService2 {
 public:
  EchoService2ClientImpl(shared_ptr<PCQueue<bool> > pcqueue) : called_(0), pcqueue_(pcqueue) {
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
    ++called_;
    VLOG(2) << "CallEcho2Done response:" << response->text() << called_;
    pcqueue_->Push(called_ < FLAGS_num_threads);
  }
  int called() const {
    return called_;
  }
 private:
  Connection *connection_;
  int called_;
  shared_ptr<PCQueue<bool> > pcqueue_;
};

void ClientCallMultiThreadDone(
    shared_ptr<RpcController> controller,
    shared_ptr<Hello::EchoRequest> request,
    shared_ptr<Hello::EchoResponse> response) {
  VLOG(2) << "ClientCallMultiThreadDone called";
  CHECK_EQ("server->" + request->question(), response->text());
}

void ClientThreadRun(shared_ptr<Hello::EchoService2::Stub> client_stub) {
  shared_ptr<Hello::EchoRequest> request(new Hello::EchoRequest);
  shared_ptr<Hello::EchoResponse> response(new Hello::EchoResponse);
  request->set_question("client question");
  shared_ptr<RpcController> controller(new RpcController);
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
  shared_ptr<ClientConnection> client_connection;
  shared_ptr<Hello::EchoService2::Stub> client_stub;
  shared_ptr<PCQueue<bool> > pcqueue(new PCQueue<bool>);
  shared_ptr<EchoService2ClientImpl> echo_service;
  echo_service.reset(new EchoService2ClientImpl(pcqueue));
  VLOG(2) << "client connection, use count: " << client_connection.use_count();
   VLOG(2) << "New client connection";
  client_connection.reset(new ClientConnection(FLAGS_address, FLAGS_port));
  VLOG(2) << "Register service";
  client_connection->RegisterService(echo_service.get());
  VLOG(2) << "client connection, use count: " << client_connection.use_count();
  client_stub.reset(new Hello::EchoService2::Stub(client_connection.get()));
  CHECK(client_connection->Connect());
  // Create a pool of threads to run all of the io_services.
  vector<shared_ptr<boost::thread> > threads;
  VLOG(2) << "client connection, use count: " << client_connection.use_count();
  for (size_t i = 0; i < FLAGS_num_threads; ++i) {
    VLOG(2) << "client connection, use count: " << client_connection.use_count();
    ClientThreadRun(client_stub);
    VLOG(2) << "client connection, use count: " << client_connection.use_count();
  }
  VLOG(2) << "client connection, use count: " << client_connection.use_count();
  int cnt = 0;
  while (pcqueue->Pop()) {
    ++cnt;
    VLOG(2) << "Get on pop" << cnt;
    if (cnt == FLAGS_num_threads) {
      break;
    }
  }
  CHECK_EQ(echo_service->called(), FLAGS_num_threads);
  return 0;
}
