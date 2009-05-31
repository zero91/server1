// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP
#include <boost/thread/shared_mutex.hpp>
#include "server/full_dual_channel_proxy.hpp"
#include "server/io_service_pool.hpp"
class ClientConnection : public FullDualChannel {
 public:
  ClientConnection(const string &name, const string &server, const string &port)
    : io_service_pool_(name + ".IOService", 1),
      threadpool_(name + ".ThreadPool", kClientThreadPoolSize), server_(server), port_(port), out_threadpool_(NULL), out_io_service_pool_(NULL) {
      VLOG(2) << "Constructor client connection";
    connection_template_.set_name(server + "::" + port + "::Client");
  }
  virtual bool RegisterService(google::protobuf::Service *service) {
    return connection_template_.RegisterService(service);
  }
  virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) {
    proxy_->CallMethod(method, controller, request, response, done);
  }

  const string name() {
    return connection_template_.name();
  }
  void set_io_service_pool(IOServicePool *io_service_pool) {
    out_io_service_pool_ = io_service_pool;
  }

  void set_threadpool(ThreadPool *out_threadpool) {
    out_threadpool_ = out_threadpool;
  }
  bool IsConnected() {
    return proxy_.get() && proxy_->IsConnected();
  }
  boost::signals2::signal<void()> *close_signal() {
    return proxy_.get() ? proxy_->close_signal() : NULL;
  }
  bool Connect();
  void Disconnect() {
    if (proxy_.get() && proxy_->IsConnected()) {
      VLOG(2) << "Disconnect: " << name();
      proxy_->Disconnect();
      notifier_->Wait();
      VLOG(2) << "Disconnect after notifer wait: " << name();
    }
    if (out_threadpool_ == NULL) {
      threadpool_.Stop();
    }
    if (out_io_service_pool_ == NULL) {
      io_service_pool_.Stop();
    }
  }
  ~ClientConnection() {
    CHECK(!IsConnected());
    CHECK(!io_service_pool_.IsRunning());
    CHECK(!threadpool_.IsRunning());
    VLOG(2) << "~ClientConnection";
  }
 private:
  void ConnectionClose();
  boost::asio::io_service &GetIOService() {
    if (out_io_service_pool_) {
      return out_io_service_pool_->get_io_service();
    }
    return io_service_pool_.get_io_service();
  }

  static const int kClientThreadPoolSize = 1;
  boost::shared_ptr<FullDualChannelProxy> proxy_;
  ProtobufConnection connection_template_;
  IOServicePool io_service_pool_;
  ThreadPool threadpool_;
  ThreadPool *out_threadpool_;
  IOServicePool *out_io_service_pool_;
  string server_, port_;
  boost::shared_ptr<Notifier> notifier_;
};
#endif  // CLIENT_CONNECTION_HPP
