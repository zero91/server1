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
#include "services/file_transfer/checkbook.hpp"
#include "server/server.hpp"
#include "server/client_connection.hpp"
#include "net/mac_address.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem/operations.hpp>
#include <sstream>
#include <gflags/gflags.h>
namespace {
static const char *kTestFile = "downloadtest.2";
}
DEFINE_string(address, "localhost", "The test server");
DEFINE_string(port, "7890", "The test server");
DEFINE_int32(num_threads, 1, "The test server thread number");
DEFINE_int32(num_connections, 80, "The test server thread number");
DEFINE_string(local_root, ".", "The document root of the file transfer");
//DECLARE_bool(logtostderr);
//DECLARE_int32(v);
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
int main(int argc, char* argv[]) {
  FLAGS_v = 4;
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  sigset_t mask;
  sigfillset(&mask); /* Mask all allowed signals */
  int rc = pthread_sigmask(SIG_SETMASK, &mask, NULL);
  VLOG(2) << "Signal masked" << rc;
  scoped_ptr<ProtobufConnection> server_connection;
  scoped_ptr<Server> server;
  VLOG(2) << "New server connection";
  server_connection.reset(new ProtobufConnection("Server"));
  server.reset(new Server(1, FLAGS_num_threads));
  const int kConnectionNumber = FLAGS_num_connections;
  const int kSliceNumber = 100;
  const int kFileSize = CheckBook::GetSliceSize()  * kSliceNumber + 1;
  string content;
  CreateFile(kFileSize, &content);
  const string local_filename = "111";
  FileTransfer::RegisterRequest request;
  FileTransfer::RegisterResponse response;
  request.set_src_filename(kTestFile);
  request.set_local_filename(local_filename);
  request.set_local_mac_address(GetMacAddress());
  RpcController controller;
  scoped_ptr<FileTransferServiceImpl> local_file_transfer_service(
      new FileTransferServiceImpl(FLAGS_local_root));
  scoped_ptr<FileDownloadNotifyImpl> local_file_notify(
      new FileDownloadNotifyImpl);
  boost::shared_ptr<FileDownloadNotifier> notifier(
      new FileDownloadNotifier("FinishedNotify"));
  local_file_notify->RegisterNotifier(kTestFile, local_filename, notifier);
  vector<boost::shared_ptr<ClientConnection> > connections;
  for (int i = 0; i < kConnectionNumber; ++i) {
    const string name("FileDownloadTest2Client." + boost::lexical_cast<string>(i));
    boost::shared_ptr<ClientConnection> r(new ClientConnection(name, FLAGS_address, FLAGS_port));
    CHECK(!r->IsConnected());
    CHECK(r->Connect());
    r->RegisterService(local_file_transfer_service.get());
    r->RegisterService(local_file_notify.get());
    connections.push_back(r);
    FileTransfer::FileDownloadService::Stub stub(r.get());
    request.set_peer_name(r->name());
    stub.RegisterDownload(
        &controller,
        &request, &response, NULL);
    controller.Wait();
    CHECK(response.succeed());
  }
  for (int i = 0; i < connections.size(); ++i) {
    if (i % 2 == 1) {
      VLOG(2) << "Disconnect: " << i << connections[i]->name();
      connections[i]->Disconnect();
      connections.erase(connections.begin() + i);
    }
  }
  CHECK_LT(connections.size(), kConnectionNumber);
  CHECK_GT(connections.size(), 0);
  VLOG(2) << "connections size: " << connections.size();
  for (int i = 0; i < connections.size(); ++i) {
    CHECK(connections[i]->IsConnected());
  }
  notifier->Wait();
  for (int i = 0; i < connections.size(); ++i) {
    VLOG(2) << "Disconnect: " << i << connections[i]->name();
    connections[i]->Disconnect();
  }
  boost::filesystem::path dest_path(FLAGS_local_root);
  dest_path /= local_filename;
  return 0;
}
