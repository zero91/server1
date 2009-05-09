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
    done->Run();
  }
};


class EchoTest : public testing::Test {
 public:
  EchoTest()
    : server_connection_(new ProtobufConnection),
      client_io_service_(new boost::asio::io_service),
      server_(FLAGS_server, FLAGS_port, FLAGS_num_threads,
              server_connection_),
      client_connection_(new ClientConnection(
          client_io_service_,
          FLAGS_server, FLAGS_port)),
      stub_(client_connection_.get()),
      server_thread_(boost::bind(&Server::Run, &server_)) {
    VLOG(2) << "Register service";
    server_connection_->RegisterService(&echo_service_);
    CHECK(!client_connection_->IsConnected());
    CHECK(client_connection_->Connect());
    done_.reset(google::protobuf::NewPermanentCallback(
      this, &EchoTest::CallDone));
  }

  void CallDone() {
    LOG(INFO) << "Call done is called";
    client_io_service_->stop();
  }
 protected:
  shared_ptr<ProtobufConnection> server_connection_;
  shared_ptr<boost::asio::io_service> client_io_service_;
  shared_ptr<ClientConnection> client_connection_;
  Server server_;
  EchoServiceImpl echo_service_;
  Hello::EchoService::Stub stub_;
  scoped_ptr<google::protobuf::Closure> done_;
  boost::thread server_thread_;
};

TEST_F(EchoTest, Test1) {
  Hello::EchoRequest request;
  Hello::EchoResponse response;
  request.set_question("hello");
  RpcController controller;
  stub_.Echo(&controller,
              &request,
              &response,
              done_.get());
  client_io_service_->run();
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
