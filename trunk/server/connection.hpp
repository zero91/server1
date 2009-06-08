// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef NET2_CONNECTION_HPP_
#define NET2_CONNECTION_HPP_
#include "base/base.hpp"
#include "glog/logging.h"
#include "server/meta.pb.h"
#include "protobuf/service.h"
#include "boost/thread.hpp"
#include "server/shared_const_buffers.hpp"
#include "boost/signals2/signal.hpp"
#include "server/raw_connection.hpp"
#include <protobuf/message.h>
#include <protobuf/descriptor.h>
#include <protobuf/service.h>
#include "thread/notifier.hpp"

class RpcController : virtual public google::protobuf::RpcController {
 public:
  RpcController(const string name = "NoNameRpcController") : notifier_(new Notifier(name)) {
  }
  void Reset() {
    failed_.clear();
    notifier_.reset(new Notifier("RPCController"));
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

class Connection : virtual public RpcController,
  virtual public google::protobuf::RpcChannel,
  public boost::enable_shared_from_this<Connection> {
 public:
  class AsyncCloseListener {
   public:
    virtual void ConnectionClosed(Connection *) = 0;
  };
  Connection(const string &name) : RpcController(name), name_(name), impl_(NULL), id_(++global_connection_id), listen_not_notified_(0) {
  }

  virtual bool RegisterService(google::protobuf::Service *service) = 0;
  virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) = 0;

  bool RegisterAsyncCloseListener(
      boost::shared_ptr<AsyncCloseListener> listener) {
    boost::shared_lock<boost::shared_mutex> locker(mutex_);
    listeners_.push_back(listener);
    return true;
  }

  virtual void Disconnect() {
    VLOG(2) << "Disconnect " << name();
    boost::unique_lock<boost::shared_mutex> locker(mutex_);
    if (impl_) {
      impl_->Disconnect();
    }
    impl_ = NULL;
    VLOG(2) << "Disconnected " << name();
  }

  virtual bool IsConnected() const {
    return impl_ && impl_->IsConnected();
  }
  const string name() const {
    return impl_ ? impl_->name() : name_;
  }
  virtual ~Connection() {
    CHECK(!IsConnected());
  }
  inline bool ScheduleWrite() {
    boost::shared_lock<boost::shared_mutex> locker(mutex_);
    return impl_ && impl_->ScheduleWrite();
  }
  template <typename T>
  // The push will take the ownership of the data
  inline bool PushData(const T &data) {
    boost::shared_lock<boost::shared_mutex> locker(mutex_);
    if (impl_) {
      impl_->PushData(data);
      return true;
    }
    return false;
  }
  // Create a connection from a socket.
  // The protocol special class should implment it.
  virtual boost::shared_ptr<Connection> Span(boost::asio::ip::tcp::socket *socket) = 0;
 protected:
  static int global_connection_id;
  void ImplClosed() {
    VLOG(2) << name_ << "ImplClosed";
    mutex_.lock();
    impl_ = NULL;
    mutex_.unlock();
    if (atomic_compare_and_swap(&listen_not_notified_, 0, 1)) {
      for (int i = 0; i < listeners_.size(); ++i) {
        VLOG(2) << "listener " << i << " expired: " << listeners_[i].expired();
        if (!listeners_[i].expired()) {
          listeners_[i].lock()->ConnectionClosed(this);
        }
      }
      listeners_.clear();
    }
  }
  string name_;
  RawConnection *impl_;
  boost::shared_mutex mutex_;
  friend class RawConnection;
  int id_;
  volatile int listen_not_notified_;
  vector<boost::weak_ptr<AsyncCloseListener> > listeners_;
};
#endif // NET2_CONNECTION_HPP_
