#include "client/client_connection.hpp"
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
 public:
  EchoService2Impl() : called_(false) {
  }
  virtual void Echo1(google::protobuf::RpcController *controller,
                    const Hello::EchoRequest *request,
                    Hello::EchoResponse *response,
                    google::protobuf::Closure *done) {
    LOG(INFO) << "Echo ServiceImpl called";
    response->set_echoed(1);
    response->set_text("server->" + request->question());
    done->Run();
    EchoService2::Stub stub(
        dynamic_cast<google::protobuf::RpcChannel*>(controller));
    Hello::EchoRequest2  *request2 = new Hello::EchoRequest2;
    Hello::EchoResponse2 *response2 = new Hello::EchoResponse2;
    request2->set_question("server question");
    RpcController controller2;
    google::protobuf::Closure *done2 = google::protobuf::NewCallback(
        this, &EchoService2Impl::CallEcho2Done, request2, response2);
    stub.Echo2(&controller2,
               request2,
               response2,
               done2);
    connection_ = dynamic_cast<Connection*>(controller);
  }
  virtual void Echo2(google::protobuf::RpcController *controller,
                     const Hello::EchoRequest2 *request,
                    Hello::EchoResponse2 *response,
                    google::protobuf::Closure *done) {
    LOG(INFO) << "Echo2 called";
    response->set_echoed(2);
    response->set_text("client->" + request->question());
    done->Run();
  }
  void CallEcho2Done(Hello::EchoRequest2 *request,
                     Hello::EchoResponse2 *response) {
    VLOG(2) << "CallEcho2Done response:" << response->text();
    CHECK_EQ("client->" + request->question(), response->text());
    delete response;
    delete request;
    connection_->Close();
    called_ = true;
  }
  bool called() const {
    return called_;
  }
 private:
  Connection *connection_;
  bool called_;
};

class ListenTest : public testing::Test {
 public:
  ListenTest()
    : server_connection_(new ProtobufConnection),
      server_(FLAGS_server, FLAGS_port, FLAGS_num_threads,
              server_connection_),
      client_io_service_(new boost::asio::io_service),
      client_connection_(new ClientConnection(
          client_io_service_, FLAGS_server, FLAGS_port)),
      client_stub_(client_connection_.get()),
      server_thread_(boost::bind(&Server::Run, &server_)) {
    VLOG(2) << "Register service";
    server_connection_->RegisterService(&echo_service_);
    client_connection_->RegisterService(&echo_service_);
    CHECK(!client_connection_->IsConnected());
    CHECK(client_connection_->Connect());
  }

  void ClientCallDone() {
    VLOG(2) << "ClientCallDone";
  }
 protected:
  shared_ptr<ProtobufConnection> server_connection_;
  shared_ptr<boost::asio::io_service> client_io_service_;
  Server server_;
  shared_ptr<ClientConnection> client_connection_;
  RpcController listener_controller_;
  boost::thread server_thread_;
  Hello::EchoService2::Stub client_stub_;
  EchoService2Impl echo_service_;
};

TEST_F(ListenTest, Test1) {
  Hello::EchoRequest request;
  Hello::EchoResponse response;
  request.set_question("client question");
  RpcController controller;
  client_stub_.Echo1(&controller,
                      &request,
                      &response,
                      NewClosure(boost::bind(
                          &ListenTest::ClientCallDone, this)));
  VLOG(2) << response.text() << " wait for server terminate";
  client_io_service_->run();
  CHECK_EQ("server->" + request.question(), response.text());
  CHECK(echo_service_.called());
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
