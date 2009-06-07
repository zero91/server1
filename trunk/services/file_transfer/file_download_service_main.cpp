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
#include "services/file_transfer/file_download_service.hpp"
#include "services/file_transfer/file_transfer_service.hpp"
#include "services/file_transfer/checkbook.hpp"
#include "server/server.hpp"
#include "server/client_connection.hpp"
#include "net/mac_address.hpp"
#include <sstream>
#include <gflags/gflags.h>
DEFINE_string(address, "localhost", "The test server");
DEFINE_string(port, "7890", "The test server");
DEFINE_int32(num_threads, 1, "The test server thread number");
DEFINE_string(doc_root, ".", "The document root of the file transfer");
DECLARE_bool(logtostderr);
DECLARE_int32(v);
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
  FileDownloadServiceImpl file_download_service(FLAGS_doc_root, FLAGS_num_threads);
  server_connection->RegisterService(&file_download_service);
  server->Listen(FLAGS_address, FLAGS_port, server_connection.get());
  sigset_t new_mask;
  sigfillset(&new_mask);
  sigset_t old_mask;
  sigset_t wait_mask;
  sigemptyset(&wait_mask);
  sigaddset(&wait_mask, SIGINT);
  sigaddset(&wait_mask, SIGQUIT);
  sigaddset(&wait_mask, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);
  pthread_sigmask(SIG_SETMASK, &old_mask, 0);
  // Wait for signal indicating time to shut down.
  pthread_sigmask(SIG_BLOCK, &wait_mask, 0);
  int sig = 0;
  sigwait(&wait_mask, &sig);
  VLOG(2) << "Catch signal" << sig;
  server->Stop();
  return 0;
}
