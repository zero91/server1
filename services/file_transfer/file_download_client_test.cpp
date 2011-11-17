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
#include "services/file_transfer/file_download_service.hpp"
#include "services/file_transfer/file_transfer_service.hpp"
#include "services/file_transfer/file_download_client.hpp"
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
DEFINE_string(local_root, ".", "The document root of the file transfer");
DEFINE_int32(num_connections, 80, "The test connections number");
DECLARE_bool(logtostderr);
DECLARE_int32(v);

namespace {
static const char *kTestFile = "checkbooktest.1";
}

class FileDownloadClientTest : public testing::Test {
 public:
  void SetUp() {
    server_connection_.reset(new ProtobufConnection("Server"));
    server_.reset(new Server(1, FLAGS_num_threads));
    VLOG(2) << "New file_download_client_ connection";
    file_download_service_.reset(new FileDownloadServiceImpl(FLAGS_doc_root, FLAGS_num_threads));
    server_connection_->RegisterService(file_download_service_.get());
    server_->Listen(FLAGS_server, FLAGS_port, server_connection_.get());
    file_download_client_.reset(
        new FileDownloadClient(FLAGS_doc_root,
                               FLAGS_server, FLAGS_port, FLAGS_num_threads));
  }
  void TearDown() {
    VLOG(2) << "Reset server connection";
    file_download_client_->Stop();
    file_download_service_->Stop();
    server_->Stop();
    server_.reset();
    server_connection_.reset();
    VLOG(2) << "Reset file_download_client_ connection";
    file_download_service_.reset();
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
  boost::shared_ptr<FileDownloadServiceImpl> file_download_service_;
  boost::shared_ptr<FileDownloadClient> file_download_client_;
};

TEST_F(FileDownloadClientTest, Test1) {
  const int kFileSize = CheckBook::GetSliceSize() + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string local_filename = "1112";
  ASSERT_TRUE(file_download_client_->Start());
  ASSERT_TRUE(file_download_client_->DownloadFile(kTestFile, local_filename));
  while (1) {
    vector<FileDownloadClient::PercentItem> percents;
    file_download_client_->Percent(&percents);
    CHECK_EQ(percents.size(), 1);
    if (percents[0].percent == 1000) {
      break;
    }
    VLOG(2) << "Percent: " << percents[0].percent;
    sleep(1);
  }
  file_download_client_->Stop();
  boost::filesystem::path dest_path(FLAGS_local_root);
  dest_path /= local_filename;
  ASSERT_TRUE(FileEqual(kTestFile, dest_path.file_string()));
//  boost::filesystem::remove(kTestFile);
//  boost::filesystem::remove(dest_path);
}

int main(int argc, char **argv) {
  FLAGS_v = 2;
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
