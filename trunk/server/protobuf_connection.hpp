// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef NET2_PROTOBUF_CONNECTION_HPP_
#define NET2_PROTOBUF_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/connection.hpp"
#include <server/meta.pb.h>
#include <boost/function.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <glog/logging.h>
#include <protobuf/message.h>
#include <protobuf/descriptor.h>
#include <protobuf/service.h>
#include "thread/notifier.hpp"
class ProtobufConnectionImpl;
class RpcController : virtual public google::protobuf::RpcController {
 public:
  RpcController() : notifier_(new Notifier) {
  }
  void Reset() {
    failed_.clear();
    notifier_.reset(new Notifier);
  }
  void SetFailed(const string &failed) {
    failed_ = failed;
  }
  bool Failed() const {
    return !failed_.empty();
  }
  string ErrorText() const {
    return failed_;
  }
  void StartCancel() {
  }
  bool IsCanceled() const {
    return false;
  }
  void NotifyOnCancel(google::protobuf::Closure *callback) {
  }
  bool Wait() {
    return notifier_->Wait();
  }
  bool Wait(int timeout_ms) {
    return notifier_->Wait(timeout_ms);
  }
  void Notify() {
    notifier_->Notify();
  }
 private:
  string failed_;
  bool responsed_;
  boost::shared_ptr<Notifier> notifier_;
};

class FullDualChannel : virtual public RpcController,
  virtual public google::protobuf::RpcChannel {
 public:
  virtual bool RegisterService(google::protobuf::Service *service) = 0;
  virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) = 0;
};

class ProtobufConnection : public Connection, virtual public FullDualChannel {
 public:
  explicit ProtobufConnection(int timeout);
  ProtobufConnection();
  bool RegisterService(google::protobuf::Service *service);
  void CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done);
};
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
