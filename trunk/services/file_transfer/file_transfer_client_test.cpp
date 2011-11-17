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
#include "services/file_transfer/file_transfer_client.hpp"
#include "services/file_transfer/checkbook.hpp"
#include "server/server.hpp"
#include "server/client_connection.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem/operations.hpp>
#include <sstream>
#include <gflags/gflags.h>
#include <gtest/gtest.h>
DEFINE_string(server, "localhost", "The test server");
DEFINE_string(port, "6789", "The test server");
DEFINE_int32(num_connections, 80, "The test connections number");
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
    client_connection_.reset(new ClientConnection("FileTransferMainClient", FLAGS_server, FLAGS_port));
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
    client_connection_->Disconnect();
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
  boost::shared_ptr<ProtobufConnection> server_connection_;
  boost::shared_ptr<Server> server_;
  boost::shared_ptr<ClientConnection> client_connection_;
  boost::shared_ptr<FileTransfer::FileTransferService::Stub> client_stub_;
  boost::shared_ptr<FileTransferServiceImpl> file_transfer_service_;
  boost::shared_ptr<FileTransferClient> file_transfer_client_;
};

TEST_F(FileTransferTest, Test1) {
  const int kFileSize = CheckBook::GetSliceSize() + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  file_transfer_client_.reset(FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  boost::shared_ptr<Notifier> ns(new Notifier("NotifyTest1"));
  file_transfer_client_->set_finish_listener(ns->notify_handler());
  file_transfer_client_->Start();
  VLOG(2) << "Start";
  CHECK(!client_connection_->IsConnected());
  CHECK(client_connection_->Connect());
  file_transfer_client_->PushChannel(client_connection_.get());
  ns->Wait();
  boost::filesystem::path dest_path(FLAGS_doc_root);
  dest_path /= dest_filename;
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
  client_connection_->Disconnect();
  file_transfer_client_->Stop();
  boost::filesystem::remove(kTestFile);
  boost::filesystem::remove(dest_path);
}

TEST_F(FileTransferTest, Test2) {
  const int kSliceNumber = 10;
  const int kFileSize = CheckBook::GetSliceSize()  * kSliceNumber + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  file_transfer_client_.reset(FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  boost::shared_ptr<Notifier> ns(new Notifier("NotifyTest2"));
  file_transfer_client_->set_finish_listener(ns->notify_handler());
  file_transfer_client_->Start();
  CHECK(!client_connection_->IsConnected());
  CHECK(client_connection_->Connect());
  file_transfer_client_->PushChannel(client_connection_.get());
  ns->Wait();
  boost::filesystem::path dest_path(FLAGS_doc_root);
  dest_path /= dest_filename;
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
  client_connection_->Disconnect();
  file_transfer_client_->Stop();
  boost::filesystem::remove(kTestFile);
  boost::filesystem::remove(dest_path);
}

TEST_F(FileTransferTest, Test3) {
  const int kConnectionNumber = FLAGS_num_connections;
  const int kSliceNumber = 10;
  const int kFileSize = CheckBook::GetSliceSize()  * kSliceNumber + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  file_transfer_client_.reset(FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  boost::shared_ptr<Notifier> ns(new Notifier("NotifyTest3"));
  file_transfer_client_->set_finish_listener(ns->notify_handler());
  file_transfer_client_->Start();
  vector<boost::shared_ptr<ClientConnection> > connections;
  for (int i = 0; i < kConnectionNumber; ++i) {
    boost::shared_ptr<ClientConnection> r(new ClientConnection(
        "Test3." + boost::lexical_cast<string>(i), FLAGS_server, FLAGS_port));
    CHECK(!r->IsConnected());
    CHECK(r->Connect());
    VLOG(2) << "Create client connection: " << r->name();
    connections.push_back(r);
    file_transfer_client_->PushChannel(r.get());
  }
  ns->Wait();
  boost::filesystem::path dest_path(FLAGS_doc_root);
  dest_path /= dest_filename;
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
  for (int i = 0; i < kConnectionNumber; ++i) {
    connections[i]->Disconnect();
  }
  file_transfer_client_->Stop();
  boost::filesystem::remove(kTestFile);
  boost::filesystem::remove(dest_path);
}

TEST_F(FileTransferTest, Test4) {
  const int kConnectionNumber = 20;
  const int kSliceNumber = 10;
  const int kFileSize = CheckBook::GetSliceSize()  * kSliceNumber + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  file_transfer_client_.reset(FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  boost::shared_ptr<Notifier> ns(new Notifier("NotifyTest4"));
  file_transfer_client_->set_finish_listener(ns->notify_handler());
  file_transfer_client_->Start();
  vector<boost::shared_ptr<ClientConnection> > connections;
  for (int i = 0; i < kConnectionNumber; ++i) {
    boost::shared_ptr<ClientConnection> r(new ClientConnection(
        "Test4." + boost::lexical_cast<string>(i), FLAGS_server, FLAGS_port));
    CHECK(!r->IsConnected());
    CHECK(r->Connect());
    connections.push_back(r);
    file_transfer_client_->PushChannel(r.get());
  }
  ns->Wait();
  boost::filesystem::path dest_path(FLAGS_doc_root);
  dest_path /= dest_filename;
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
  for (int i = 0; i < kConnectionNumber; ++i) {
    connections[i]->Disconnect();
  }
  file_transfer_client_->Stop();
  boost::filesystem::remove(kTestFile);
  boost::filesystem::remove(dest_path);
}

TEST_F(FileTransferTest, Test5) {
  const int kConnectionNumber = 20;
  const int kSliceNumber = 10;
  const int kFileSize = CheckBook::GetSliceSize()  * kSliceNumber + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  file_transfer_client_.reset(FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  boost::shared_ptr<Notifier> ns(new Notifier("NotifyTest5"));
  file_transfer_client_->set_finish_listener(ns->notify_handler());
  file_transfer_client_->Start();
  vector<boost::shared_ptr<ClientConnection> > connections;
  for (int i = 0; i < kConnectionNumber; ++i) {
    boost::shared_ptr<ClientConnection> r(new ClientConnection(
        "Test5." + boost::lexical_cast<string>(i), FLAGS_server, FLAGS_port));
    CHECK(!r->IsConnected());
    CHECK(r->Connect());
    connections.push_back(r);
    file_transfer_client_->PushChannel(r.get());
  }
  while (file_transfer_client_->Percent() == 0) {
    sleep(1);
  }
  VLOG(2) << "Percent: " << file_transfer_client_->Percent();
  ASSERT_GT(file_transfer_client_->Percent(), 0);
  ASSERT_LT(file_transfer_client_->Percent(), 1000);
  file_transfer_client_->Stop();
  boost::shared_ptr<FileTransferClient> file_transfer_client2(
      FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  ASSERT_EQ(file_transfer_client_->Percent(),
            file_transfer_client2->Percent());
  file_transfer_client2->set_finish_listener(ns->notify_handler());
  file_transfer_client2->Start();
  for (int i = 0; i < kConnectionNumber; ++i) {
    file_transfer_client2->PushChannel(connections[i].get());
  }
  ns->Wait();
  ASSERT_EQ(file_transfer_client2->Percent(), 1000);
  boost::filesystem::path dest_path(FLAGS_doc_root);
  dest_path /= dest_filename;
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
  for (int i = 0; i < kConnectionNumber; ++i) {
    connections[i]->Disconnect();
  }
  file_transfer_client2->Stop();
  boost::filesystem::remove(kTestFile);
  boost::filesystem::remove(dest_path);
}

TEST_F(FileTransferTest, Test6) {
  const int kConnectionNumber = FLAGS_num_connections;
  const int kSliceNumber = 10;
  const int kFileSize = CheckBook::GetSliceSize()  * kSliceNumber + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  file_transfer_client_.reset(FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  boost::shared_ptr<Notifier> ns(new Notifier("NotifyTest6"));
  file_transfer_client_->Start();
  vector<boost::shared_ptr<ClientConnection> > connections;
  for (int i = 0; i < kConnectionNumber; ++i) {
    boost::shared_ptr<ClientConnection> r(new ClientConnection(
        "Test6." + boost::lexical_cast<string>(i), FLAGS_server, FLAGS_port));
    CHECK(!r->IsConnected());
    CHECK(r->Connect());
    connections.push_back(r);
    file_transfer_client_->PushChannel(r.get());
  }
  connections[0]->Disconnect();
  connections.erase(connections.begin());
  for (int i = 0; i < connections.size(); ++i) {
    ASSERT_TRUE(connections[i]->IsConnected());
  }
  while (file_transfer_client_->Percent() == 0) {
    sleep(1);
  }
  VLOG(2) << "Percent: " << file_transfer_client_->Percent();
  ASSERT_GT(file_transfer_client_->Percent(), 0);
  ASSERT_LT(file_transfer_client_->Percent(), 1000);
  file_transfer_client_->Stop();
  VLOG(2) << "file transfer client stop";
  boost::shared_ptr<FileTransferClient> file_transfer_client2(
      FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  ASSERT_EQ(file_transfer_client_->Percent(),
            file_transfer_client2->Percent());
  file_transfer_client2->set_finish_listener(ns->notify_handler());
  file_transfer_client2->Start();
  VLOG(2) << "file transfer client2 start";
  for (int i = 0; i < connections.size(); ++i) {
    file_transfer_client2->PushChannel(connections[i].get());
  }
  ns->Wait();
  ASSERT_EQ(file_transfer_client2->Percent(), 1000);
  VLOG(2) << "connection size: " << connections.size();
  for (int i = 0; i < connections.size(); ++i) {
    connections[i]->Disconnect();
    VLOG(2) << "Disconnect " << i;
  }
  file_transfer_client2->Stop();
  VLOG(2) << "file transfer client2 stop";
  boost::filesystem::path dest_path(FLAGS_doc_root);
  dest_path /= dest_filename;
  VLOG(2) << "Compare file: " << kTestFile << " and " << dest_path.file_string();
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
  boost::filesystem::remove(kTestFile);
  boost::filesystem::remove(dest_path);
  VLOG(2) << "Removed files";
}

TEST_F(FileTransferTest, Test7) {
  const int kConnectionNumber = FLAGS_num_connections;
  const int kSliceNumber = 10;
  const int kFileSize = CheckBook::GetSliceSize()  * kSliceNumber + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string dest_filename = "111";
  // Send the checkbook.
  file_transfer_client_.reset(FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  boost::shared_ptr<Notifier> ns(new Notifier("NotifyTest7"));
  file_transfer_client_->Start();
  vector<boost::shared_ptr<ClientConnection> > connections;
  for (int i = 0; i < kConnectionNumber; ++i) {
    boost::shared_ptr<ClientConnection> r(new ClientConnection(
        "Test7." + boost::lexical_cast<string>(i), FLAGS_server, FLAGS_port));
    CHECK(!r->IsConnected());
    CHECK(r->Connect());
    connections.push_back(r);
    file_transfer_client_->PushChannel(r.get());
  }
  for (int i = 0; i < connections.size(); ++i) {
    if (i % 2 == 1) {
      VLOG(2) << "Disconnect: " << i << connections[i]->name();
      connections[i]->Disconnect();
      connections.erase(connections.begin() + i);
    }
  }
  ASSERT_LT(connections.size(), kConnectionNumber);
  ASSERT_GT(connections.size(), 0);
  VLOG(2) << "connections size: " << connections.size();
  for (int i = 0; i < connections.size(); ++i) {
    ASSERT_TRUE(connections[i]->IsConnected());
  }
  while (file_transfer_client_->Percent() == 0) {
    sleep(1);
  }
  VLOG(1) << "Percent: " << file_transfer_client_->Percent();
  ASSERT_GT(file_transfer_client_->Percent(), 0);
  ASSERT_LT(file_transfer_client_->Percent(), 1000);
  file_transfer_client_->Stop();
  VLOG(1) << "file transfer client stop";
  boost::shared_ptr<FileTransferClient> file_transfer_client2(
      FileTransferClient::Create(
      FLAGS_server, FLAGS_port, kTestFile, dest_filename, FLAGS_num_threads));
  ASSERT_EQ(file_transfer_client_->Percent(),
            file_transfer_client2->Percent());
  file_transfer_client2->set_finish_listener(ns->notify_handler());
  file_transfer_client2->Start();
  VLOG(2) << "file transfer client2 start";
  for (int i = 0; i < connections.size(); ++i) {
    file_transfer_client2->PushChannel(connections[i].get());
  }
  ns->Wait();
  ASSERT_EQ(file_transfer_client2->Percent(), 1000);
  boost::filesystem::path dest_path(FLAGS_doc_root);
  dest_path /= dest_filename;
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
  for (int i = 0; i < connections.size(); ++i) {
    VLOG(2) << "Disconnect: " << i << connections[i]->name();
    connections[i]->Disconnect();
  }
  file_transfer_client2->Stop();
  boost::filesystem::remove(kTestFile);
  boost::filesystem::remove(dest_path);
}

int main(int argc, char **argv) {
  FLAGS_v = 2;
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
