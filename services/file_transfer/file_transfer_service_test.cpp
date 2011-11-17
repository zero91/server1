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



#include "base/base.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <glog/logging.h>
#include <zlib.h>
#include "services/file_transfer/file_transfer_service.hpp"
#include "services/file_transfer/checkbook.hpp"
#include "server/server.hpp"
#include "server/client_connection.hpp"
#include "net/mac_address.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem/operations.hpp>
#include <sstream>
#include <gflags/gflags.h>
#include <gtest/gtest.h>
DEFINE_string(server, "localhost", "The test server");
DEFINE_string(port, "6789", "The test server");
DEFINE_int32(num_threads, 4, "The test server thread number");
DEFINE_string(doc_root, ".", "The document root of the file transfer");
DECLARE_bool(logtostderr);
DECLARE_int32(v);

namespace {
static const char *kTestFile = "checkbooktest.1";
}

class FileTransferTest : public testing::Test {
 public:
  void SetUp() {
    server_connection_.reset(new ProtobufConnection("Server"));
    server_.reset(new Server(2, FLAGS_num_threads));
    VLOG(2) << "New client connection";
    client_connection_.reset(new ClientConnection("Main", FLAGS_server, FLAGS_port));
    client_stub_.reset(new FileTransfer::FileTransferService::Stub(client_connection_.get()));
    file_transfer_service_.reset(new FileTransferServiceImpl(FLAGS_doc_root));
    server_connection_->RegisterService(file_transfer_service_.get());
    server_->Listen(FLAGS_server, FLAGS_port, server_connection_.get());
  }
  void TearDown() {
    VLOG(2) << "Reset server connection";
    server_->Stop();
    server_.reset();
    server_connection_.reset();
    client_stub_.reset();
    VLOG(2) << "Reset client connection";
    client_connection_.reset();
    file_transfer_service_.reset();
  }

  bool FileEqual(const string &filename1, const string &filename2) {
    VLOG(2) << "Compare " << filename1 << " and " << filename2;
    boost::iostreams::mapped_file_source file1;
    file1.open(filename1);
    if (!file1.is_open()) {
      LOG(WARNING) << "FileEqual fail to open file: " << filename1;
      return false;
    }
    boost::iostreams::mapped_file_source file2;
    file2.open(filename2);
    if (!file2.is_open()) {
      LOG(WARNING) << "FileEqual fail to open file: " << filename2;
      return false;
    }
    if (file1.size() != file2.size()) {
      LOG(WARNING) << "size is not equal";
      return false;
    }
    for (int i = 0; i < file1.size(); ++i) {
      if (file1.data()[i] != file2.data()[i]) {
        LOG(WARNING) << "content at " << i << " not equal";
        return false;
      }
    }
    return true;
  }
  void CreateFile(int file_size, string *content = NULL) {
    boost::iostreams::mapped_file_params p(kTestFile);
    p.mode = std::ios_base::out | std::ios_base::trunc;
    p.new_file_size = file_size;
    boost::iostreams::mapped_file out;
    out.open(p);
    CHECK(out.is_open());
    for (int i = 0; i < file_size; ++i) {
      out.data()[i] = static_cast<char>(i);
      if (content != NULL) {
        content->push_back(static_cast<char>(i));
      }
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
  scoped_ptr<CheckBook> checkbook(CheckBook::Create(
      "localhost", "1234", kTestFile, "111"));
  const string dest_filename = FLAGS_doc_root + "/" + checkbook->GetCheckBookDestFileName();
  ASSERT_FALSE(boost::filesystem::exists(dest_filename));
  FileTransfer::CheckBook checkbook_request;
  FileTransfer::CheckBookResponse checkbook_response;
  RpcController controller;
  checkbook_request.CopyFrom(*checkbook.get());
  CHECK(!client_connection_->IsConnected());
  CHECK(client_connection_->Connect());
  client_stub_->ReceiveCheckBook(&controller,
                                 &checkbook_request,
                                 &checkbook_response,
                                 NULL);
  controller.Wait();
  client_connection_->Disconnect();
  EXPECT_TRUE(checkbook_response.succeed());
  ASSERT_TRUE(boost::filesystem::exists(dest_filename));
  boost::filesystem::remove(dest_filename);
  boost::filesystem::remove(kTestFile);
}

TEST_F(FileTransferTest, Test2) {
  const int kFileSize = CheckBook::GetSliceSize() + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  scoped_ptr<CheckBook> checkbook(CheckBook::Create(
      "localhost", "1234", kTestFile, dest_filename));
  boost::filesystem::path checkbook_dest_filename(FLAGS_doc_root);
  checkbook_dest_filename /= checkbook->GetCheckBookDestFileName();
  ASSERT_FALSE(boost::filesystem::exists(checkbook_dest_filename));
  FileTransfer::CheckBook checkbook_request;
  FileTransfer::CheckBookResponse checkbook_response;
  RpcController controller;
  checkbook_request.CopyFrom(*checkbook.get());
  CHECK(!client_connection_->IsConnected());
  CHECK(client_connection_->Connect());
  client_stub_->ReceiveCheckBook(&controller,
                                 &checkbook_request,
                                 &checkbook_response,
                                 NULL);
  controller.Wait();
  EXPECT_TRUE(checkbook_response.succeed());
  ASSERT_TRUE(boost::filesystem::exists(checkbook_dest_filename));

  FileTransfer::SliceRequest slice_request;
  FileTransfer::SliceResponse slice_response;
  slice_request.mutable_slice()->CopyFrom(checkbook->slice(0));
  slice_request.mutable_content()->assign(
      content.c_str(), CheckBook::GetSliceSize());
  controller.Reset();
  client_stub_->ReceiveSlice(&controller, &slice_request, &slice_response, NULL);
  controller.Wait();
  EXPECT_TRUE(slice_response.succeed());
  client_connection_->Disconnect();
  sleep(2);
  scoped_ptr<CheckBook> checkbook2(CheckBook::Load(checkbook_dest_filename.file_string()));
  EXPECT_TRUE(checkbook2->slice(0).finished());
  CHECK(!client_connection_->IsConnected());
  CHECK(client_connection_->Connect());
  slice_request.mutable_slice()->CopyFrom(checkbook->slice(1));
  slice_request.mutable_content()->assign(
      content.c_str() + CheckBook::GetSliceSize(), 1);
  controller.Reset();
  client_stub_->ReceiveSlice(&controller, &slice_request, &slice_response, NULL);
  controller.Wait();
  EXPECT_TRUE(slice_response.succeed());
  EXPECT_TRUE(slice_response.finished());
  client_connection_->Disconnect();
  sleep(2);
  ASSERT_FALSE(boost::filesystem::exists(checkbook_dest_filename));
  boost::filesystem::path dest_path(FLAGS_doc_root);
  dest_path /= dest_filename;
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
  boost::filesystem::remove(kTestFile);
  boost::filesystem::remove(dest_path);
}

TEST_F(FileTransferTest, Test3) {
  const int kFileSize = CheckBook::GetSliceSize() + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  scoped_ptr<CheckBook> checkbook(CheckBook::Create(
      "localhost", "1234", kTestFile, dest_filename));
  boost::filesystem::path checkbook_dest_filename(FLAGS_doc_root);
  checkbook_dest_filename /= checkbook->GetCheckBookDestFileName();
  ASSERT_FALSE(boost::filesystem::exists(checkbook_dest_filename));
  FileTransfer::CheckBook checkbook_request;
  FileTransfer::CheckBookResponse checkbook_response;
  RpcController controller;
  checkbook_request.CopyFrom(*checkbook.get());
  CHECK(!client_connection_->IsConnected());
  CHECK(client_connection_->Connect());
  client_stub_->ReceiveCheckBook(&controller,
                                 &checkbook_request,
                                 &checkbook_response,
                                 NULL);
  controller.Wait();
  EXPECT_TRUE(checkbook_response.succeed());
  ASSERT_TRUE(boost::filesystem::exists(checkbook_dest_filename));

  FileTransfer::SliceRequest slice_request;
  FileTransfer::SliceResponse slice_response;
  slice_request.mutable_slice()->CopyFrom(checkbook->slice(0));
  slice_request.mutable_content()->assign(
      content.c_str(), CheckBook::GetSliceSize());
  controller.Reset();
  client_stub_->ReceiveSlice(&controller, &slice_request, &slice_response, NULL);
  controller.Wait();
  EXPECT_TRUE(slice_response.succeed());
  scoped_ptr<CheckBook> checkbook2(CheckBook::Load(checkbook_dest_filename.file_string()));
  EXPECT_FALSE(checkbook2->slice(0).finished());
  slice_request.mutable_slice()->CopyFrom(checkbook->slice(1));
  slice_request.mutable_content()->assign(
      content.c_str() + CheckBook::GetSliceSize(), 1);
  controller.Reset();
  client_stub_->ReceiveSlice(&controller, &slice_request, &slice_response, NULL);
  controller.Wait();
  EXPECT_TRUE(slice_response.succeed());
  EXPECT_TRUE(slice_response.finished());
  client_connection_->Disconnect();
  sleep(2);
  ASSERT_FALSE(boost::filesystem::exists(checkbook_dest_filename));
  boost::filesystem::path dest_path(FLAGS_doc_root);
  dest_path /= dest_filename;
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
  boost::filesystem::remove(kTestFile);
  boost::filesystem::remove(dest_path);
}

TEST_F(FileTransferTest, Test4) {
  const int kFileSize = CheckBook::GetSliceSize() + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  scoped_ptr<CheckBook> checkbook(CheckBook::Create(
      "localhost", "1234", kTestFile, dest_filename));
  boost::filesystem::path checkbook_dest_filename(FLAGS_doc_root);
  checkbook_dest_filename /= checkbook->GetCheckBookDestFileName();
  RpcController controller;
  ASSERT_FALSE(boost::filesystem::exists(checkbook_dest_filename));
  FileTransfer::SliceRequest slice_request;
  FileTransfer::SliceResponse slice_response;
  slice_request.mutable_slice()->CopyFrom(checkbook->slice(0));
  slice_request.mutable_content()->assign(
      content.c_str(), CheckBook::GetSliceSize());
  controller.Reset();
  CHECK(!client_connection_->IsConnected());
  CHECK(client_connection_->Connect());
  client_stub_->ReceiveSlice(&controller, &slice_request, &slice_response, NULL);
  controller.Wait();
  client_connection_->Disconnect();
  EXPECT_FALSE(slice_response.succeed());
  boost::filesystem::remove(kTestFile);
}

int main(int argc, char **argv) {
  FLAGS_v = 2;
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
