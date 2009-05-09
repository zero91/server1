#include "client/channel.hpp"
#include "client/listener.hpp"
#include "server/server.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "proto/hello.pb.h"
#include "boost/thread.hpp"

DEFINE_string(server, "localhost", "The test server");
DEFINE_string(port, "6789", "The test server");
DEFINE_int32(num_threads, 1, "The test server thread number");

class EchoService2Impl : public Hello::EchoService2 {
 public:
  virtual void Echo1(FullDualChannel *channel,
                    const Hello::EchoRequest *request,
                    Hello::EchoResponse *response,
                    google::protobuf::Closure *done) {
    LOG(INFO) << "Echo ServiceImpl called";
    response->set_echoed(1);
    response->set_text(request->question());
    done->Run();
    EchoService2::Stub stub_(channel);
    Hello::EchoRequest request2;
    Hello::EchoResponse *response2 = new Hello::EchoResponse;
    request2.set_question("hello from server");
    RpcController controller;
    google::protobuf::Closure *done2 = google::protobuf::NewCallback(
        this, &EchoService2Impl::CallEcho2Done, response2);
    stub_.Echo2(&controller,
               &request2,
               response2,
               done2);
  }
  virtual void Echo2(FullDualChannel *channel,
                    const Hello::EchoRequest *request,
                    Hello::EchoResponse *response,
                    google::protobuf::Closure *done) {
    LOG(INFO) << "Echo2 called";
    response->set_echoed(2);
    response->set_text(request->question());
    done->Run();
  }
  void CallEcho2Done(Hello::EchoResponse *response) {
    delete response;
  }
};

class ListenTest : public testing::Test {
 public:
  ListenTest()
    : server_connection_(new ProtobufConnection),
      server_(FLAGS_server, FLAGS_port, FLAGS_num_threads,
              server_connection_),
      client_io_service_(new boost::asio::io_service),
      client_channel_(client_io_service_, FLAGS_server, FLAGS_port),
      client_stub_(&client_channel_),
      server_thread_(boost::bind(&Server::Run, &server_)) {
  }

 protected:
  shared_ptr<ProtobufConnection> server_connection_;
  shared_ptr<boost::asio::io_service> client_io_service_;
  Server server_;
  RpcChannel client_channel_;
  RpcController listener_controller_;
  boost::thread server_thread_;
  Hello::EchoService::Stub client_stub_;
  EchoService2Impl echo_service_;
};

TEST_F(ListenTest, Test1) {
  shared_ptr<Hello::IncRequest> inc_request(new Hello::IncRequest);
  inc_request->set_v(1);
  inc_request->set_step(2);
  Hello::IncResponse response;
  RpcController listen_controller;
  client_io_service_->run();
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
