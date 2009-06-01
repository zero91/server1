#ifndef FULL_DUAL_CHANNEL_HPP_
#define FULL_DUAL_CHANNEL_HPP_
#include "server/protobuf_connection.hpp"
class FullDualChannelProxy : virtual public FullDualChannel, public boost::enable_shared_from_this<FullDualChannelProxy> {
 public:
  static boost::shared_ptr<FullDualChannelProxy> Create(FullDualChannel *channel) {
    boost::shared_ptr<FullDualChannelProxy> proxy(
        new FullDualChannelProxy(channel));
    channel->close_signal()->connect(boost::bind(
            &FullDualChannelProxy::CloseChannel, proxy));
    return proxy;
  }
  CloseSignal* close_signal() {
    return &close_signal_;
  }
  virtual bool RegisterService(google::protobuf::Service *service) {
    mutex_.lock_shared();
    bool ret = channel_ && channel_->RegisterService(service);
    mutex_.unlock_shared();
    return ret;
  }
  virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) {
    mutex_.lock_shared();
    if (channel_ && channel_->IsConnected()) {
      channel_->CallMethod(method, controller, request, response, done);
    } else {
      RpcController *rpc_controller = dynamic_cast<RpcController*>(
          controller);
      rpc_controller->SetFailed("Connection is NULL");
      rpc_controller->Notify();
      LOG(WARNING) << "Callmethod but connection is null";
      if (done) {
        mutex_.unlock_shared();
        done->Run();
        return;
      }
    }
    mutex_.unlock_shared();
  }
  virtual void Disconnect() {
    mutex_.lock_shared();
    if (channel_) {
      channel_->Disconnect();
    }
    mutex_.unlock_shared();
  }
  virtual bool IsConnected() {
    mutex_.lock_shared();
    bool ret = channel_ && channel_->IsConnected();
    mutex_.unlock_shared();
    return ret;
  }
  virtual const string Name() {
    mutex_.lock_shared();
    const string name = channel_ != NULL ? channel_->Name() : "NoConnection";
    mutex_.unlock_shared();
    return name;
  }
 private:
  void CloseChannel() {
    mutex_.lock();
    channel_ = NULL;
    mutex_.unlock();
    close_signal_();
  }
  FullDualChannelProxy(FullDualChannel *channel) : channel_(channel) {
  }
  boost::shared_mutex mutex_;
  FullDualChannel *channel_;
  boost::signals2::signal<void()> close_signal_;
};
#endif  // FULL_DUAL_CHANNEL_HPP_
