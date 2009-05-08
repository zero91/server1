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


class ListenTest : public testing::Test {
 public:
  ListenTest()
    : connection_(new ProtobufConnection),
      client_io_service_(new boost::asio::io_service),
      server_(FLAGS_server, FLAGS_port, FLAGS_num_threads,
              connection_),
      channel_(client_io_service_, FLAGS_server, FLAGS_port),
      server_thread_(boost::bind(&Server::Run, &server_)) {
    done_.reset(google::protobuf::NewPermanentCallback(
      this, &ListenTest::CallDone));
  }

  void CallDone() {
    LOG(INFO) << "Call done is called";
    client_io_service_->stop();
  }
  void ListenInc(shared_ptr<Hello::IncResponse> response) {
    LOG(INFO) << "ListenInc is called";
  }
 protected:
  shared_ptr<ProtobufConnection> connection_;
  shared_ptr<boost::asio::io_service> client_io_service_;
  Server server_;
  RpcChannel channel_;
  RpcController listener_controller_;
  scoped_ptr<google::protobuf::Closure> done_;
  boost::thread server_thread_;
};

TEST_F(ListenTest, Test1) {
  Hello::IncRequest inc_request;
  inc_request.set_v(1);
  inc_request.set_step(2);
  Hello::IncResponse response;
  RpcController listen_controller;
  Listener listener(client_io_service_, FLAGS_server, FLAGS_port);
  listener.Listen<Hello::IncResponse>(
      boost::bind(&ListenTest::ListenInc, this, _1));
  client_io_service_->run();
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
