#ifndef CHANNEL_HPP_
#define CHANNEL_HPP_
#include <protobuf/message.h>
#include <protobuf/descriptor.h>
#include <protobuf/service.h>
#include "client/protobuf_client_connection.hpp"
class RpcController : public google::protobuf::RpcController {
 public:
  void Reset() {
    failed_.clear();
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
 private:
  string failed_;
};
class RpcChannel : public google::protobuf::RpcChannel {
 public:
  RpcChannel(IOServicePtr io_service,
          const string &server, const string &port)
    : server_(server), port_(port),
      client_(io_service) {
  }
  void CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done);
 private:
  void HandleResponse(
      const ProtobufLineFormat *line_format,
      google::protobuf::RpcController *controller,
      google::protobuf::Message *response,
      google::protobuf::Closure *done,
      shared_ptr<ProtobufEncoder> encoder
      );
  IOServicePtr io_service_;
  string server_, port_;
  ProtobufClientConnection client_;
};
#endif  // CHANNEL_HPP_
