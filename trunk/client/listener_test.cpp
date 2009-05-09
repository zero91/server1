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
    connection_->RegisterListener<Hello::IncRequest>(
        boost::bind(&ListenTest::ListenIncRequest, this, _1, _2, 10));
  }

  void ListenIncRequest(shared_ptr<Hello::IncRequest> req,
                        ProtobufReply *reply, int n) {
    LOG(INFO) << "ListenIncRequest is called" << req->v()
              << " : " << req->step();
    for (int i = 0; i < n; ++i) {
      shared_ptr<Hello::IncResponse> response(new Hello::IncResponse);
      response->set_v(req->v() + req->step() + i);
      reply->PushMessage(response);
    }
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
  boost::thread server_thread_;
};

TEST_F(ListenTest, Test1) {
  shared_ptr<Hello::IncRequest> inc_request(new Hello::IncRequest);
  inc_request->set_v(1);
  inc_request->set_step(2);
  Hello::IncResponse response;
  RpcController listen_controller;
  Listener listener(client_io_service_, FLAGS_server, FLAGS_port);
  listener.Listen<Hello::IncResponse>(
      boost::bind(&ListenTest::ListenInc, this, _1));
  listener.Send(inc_request);
  client_io_service_->run();
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
