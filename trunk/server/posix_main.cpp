#include "server/server.hpp"
#include "server/protobuf_connection.hpp"
#include <iostream>
#include <string>
#include <pthread.h>
#include <signal.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <boost/thread.hpp>
DEFINE_string(address, "localhost","The address");
DEFINE_string(port, "8888","The port");
DEFINE_int32(num_threads, 1,"The thread size");

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  // Block all signals for background thread.
  sigset_t new_mask;
  sigfillset(&new_mask);
  sigset_t old_mask;
  pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);

  // Run server in background thread.
  shared_ptr<ProtobufConnection> protobuf_connection(new ProtobufConnection);
  Server s(FLAGS_address,
           FLAGS_port,
           FLAGS_num_threads,
           protobuf_connection);
  boost::thread t(boost::bind(&Server::Run, &s));

  // Restore previous signals.
  pthread_sigmask(SIG_SETMASK, &old_mask, 0);

  // Wait for signal indicating time to shut down.
  sigset_t wait_mask;
  sigemptyset(&wait_mask);
  sigaddset(&wait_mask, SIGINT);
  sigaddset(&wait_mask, SIGQUIT);
  sigaddset(&wait_mask, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &wait_mask, 0);
  int sig = 0;
  sigwait(&wait_mask, &sig);

  // Stop the server.
  s.Stop();
  t.join();
  return 0;
}
