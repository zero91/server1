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
#include "server/timer.hpp"

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
  virtual public google::protobuf::RpcChannel, virtual public Timer,
  public boost::enable_shared_from_this<Connection> {
 private:
  static const int kTimeoutSec = 30;
 public:
  class AsyncCloseListener {
   public:
    virtual void ConnectionClosed(Connection *) = 0;
  };
  Connection(const string &name) :
    RpcController(name), name_(name), id_(++global_connection_id),
    status_(new RawConnectionStatus) {
  }

  virtual bool RegisterService(google::protobuf::Service *service) = 0;
  virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) = 0;

  bool RegisterAsyncCloseListener(boost::weak_ptr<AsyncCloseListener> listener) {
    boost::mutex::scoped_lock locker(listener_mutex_);
    listeners_.push_back(listener);
    return true;
  }

  virtual void Disconnect() {
    if (status_->closing()) {
      VLOG(2) << "Disconnect " << name() << " but is closing";
      return;
    }
    VLOG(2) << "Disconnect " << name();
    status_->mutex().lock_shared();
    if (impl_) {
      impl_->Disconnect(status_, false);
      return;
    }
    status_->mutex().unlock_shared();
    VLOG(2) << "Disconnected " << name();
  }

  virtual bool IsConnected() const {
    return !status_->idle() && !status_->closing();
  }
  const string name() const {
    return impl_.get() ? impl_->name() : name_;
  }
  virtual ~Connection() {
    CHECK(!IsConnected());
  }
  virtual int timeout() const {
    return kTimeoutSec;
  }
  virtual bool period() const {
    return IsConnected();
  }
  virtual void Expired() {
    VLOG(2) << name() << " Expired";
    status_->mutex().lock_shared();
    if (status_->closing()) {
      VLOG(2) << "Heartbeat " << name() << " but is closing";
      status_->mutex().unlock_shared();
      return;
    }
    if (impl_.get() == NULL) {
      status_->mutex().unlock_shared();
      return;
    }
    impl_->Heartbeat(status_);
  }
  inline bool ScheduleWrite() {
    RawConnectionStatus::Locker locker(status_->mutex());
    if (status_->closing()) {
      VLOG(2) << "ScheduleWrite " << name() << " but is closing";
      return false;
    }
    return impl_.get() && impl_->ScheduleWrite(status_);
  }
  template <typename T>
  // The push will take the ownership of the data
  inline bool PushData(const T &data) {
    RawConnectionStatus::Locker locker(status_->mutex());
    if (status_->closing()) {
      VLOG(2) << "PushData " << name() << " but is closing";
      return false;
    }
    return impl_.get() && impl_->PushData(data);
  }
  // Create a connection from a socket.
  // The protocol special class should implment it.
  virtual boost::shared_ptr<Connection> Span(
      boost::asio::ip::tcp::socket *socket) = 0;
 protected:
  static int global_connection_id;
  void ImplClosed() {
    VLOG(2) << name_ << "ImplClosed";
    boost::mutex::scoped_lock locker(listener_mutex_);
    for (int i = 0; i < listeners_.size(); ++i) {
      VLOG(2) << "listener " << i << " expired: " << listeners_[i].expired();
      boost::shared_ptr<AsyncCloseListener> listener = listeners_[i].lock();
      if (!listeners_[i].expired()) {
        listener->ConnectionClosed(this);
      }
    }
    listeners_.clear();
    impl_.reset();
  }
  boost::intrusive_ptr<RawConnectionStatus> status_;
  string name_;
  boost::scoped_ptr<RawConnection> impl_;
  int id_;
  vector<boost::weak_ptr<AsyncCloseListener> > listeners_;
  boost::mutex listener_mutex_;
  friend class RawConnection;
};
#endif // NET2_CONNECTION_HPP_
