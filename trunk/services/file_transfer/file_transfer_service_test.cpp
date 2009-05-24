// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include "base/base.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <glog/logging.h>
#include <zlib.h>
#include "services/file_transfer/file_transfer_service.hpp"
#include "services/file_transfer/checkbook.hpp"
#include "server/server.hpp"
#include "client/client_connection.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem/operations.hpp>
#include <sstream>
#include <gflags/gflags.h>
#include <gtest/gtest.h>
DEFINE_string(server, "localhost", "The test server");
DEFINE_string(port, "6789", "The test server");
DEFINE_int32(num_threads, 4, "The test server thread number");
DEFINE_string(doc_root, "/tmp", "The document root of the file transfer");
namespace {
static const char *kTestFile = "checkbooktest.1";
}

class FileTransferTest : public testing::Test {
 public:
  void SetUp() {
    server_connection_.reset(new ProtobufConnection);
    server_connection_->set_name("Server");
    server_.reset(new Server(2, FLAGS_num_threads));
    VLOG(2) << "New client connection";
    client_connection_.reset(new ClientConnection(FLAGS_server, FLAGS_port));
    client_connection_->set_name("Client");
    client_stub_.reset(new FileTransfer::FileTransferService::Stub(client_connection_.get()));
    file_transfer_service_.reset(new FileTransferServiceImpl(FLAGS_doc_root));
    server_connection_->RegisterService(file_transfer_service_.get());
    server_->Listen(FLAGS_server, FLAGS_port, server_connection_.get());
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
    file_transfer_service_.reset();
  }

  void CreateFile(int file_size) {
    boost::iostreams::mapped_file_params p(kTestFile);
    p.mode = std::ios_base::out | std::ios_base::trunc;
    p.new_file_size = file_size;
    boost::iostreams::mapped_file out;
    out.open(p);
    CHECK(out.is_open());
    for (int i = 0; i < file_size; ++i) {
      out.data()[i] = i;
    }
    out.close();
  }
 protected:
  boost::scoped_ptr<ProtobufConnection> server_connection_;
  boost::scoped_ptr<Server> server_;
  boost::scoped_ptr<ClientConnection> client_connection_;
  boost::shared_ptr<FileTransfer::FileTransferService::Stub> client_stub_;
  boost::scoped_ptr<FileTransferServiceImpl> file_transfer_service_;
};

TEST_F(FileTransferTest, Test1) {
  const int kFileSize = CheckBook::GetSliceSize() + 1;
  CreateFile(kFileSize);
  boost::filesystem::remove(kTestFile);
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
