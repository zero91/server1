#include "client/channel.hpp"

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                         google::protobuf::RpcController *controller,
                         const google::protobuf::Message *request,
                         google::protobuf::Message *response,
                         google::protobuf::Closure *done) {
  shared_ptr<ProtobufEncoder> encoder(new ProtobufEncoder(method->full_name(), request));
  if (!encoder->Encoded()) {
    LOG(WARNING) << "encode error!";
    controller->SetFailed("Encode failed");
    return;
  }
  if (!client_.Connect(server_, port_)) {
    LOG(WARNING) << "connect failed";
    controller->SetFailed("Fail to connection " +
                          server_ + " : " + port_);
    done->Run();
    return;
  }
  VLOG(3) << "connect to " << server_ << ":" << port_ << " success";
  client_.Send(encoder->ToBuffers());
  client_.set_listener(boost::bind(
      &RpcChannel::HandleResponse, this,
      _1,
      controller, response, done, encoder));
}

void RpcChannel::HandleResponse(
    const ProtobufLineFormat *line_format,
    google::protobuf::RpcController *controller,
    google::protobuf::Message *response,
    google::protobuf::Closure *done,
    shared_ptr<ProtobufEncoder> encoder) {
  if (line_format == NULL) {
    LOG(WARNING) << "Reply line format is null";
    controller->SetFailed("Reply line format is null.");
    if (done) done->Run();
    return;
  }
  VLOG(3) << "Handle response message "
          << response->GetDescriptor()->full_name()
          << " content: " << line_format->body
          << " size: " << line_format->body.size();
  if (!response->ParseFromArray(line_format->body.c_str(),
                                line_format->body.size())) {
    LOG(WARNING) << "Fail to parse the response, name:"
                 << line_format->name;
    controller->SetFailed("Fail to parse the response, name:" +
                          line_format->name);
    if (done) done->Run();
    return;
  }
  if (done) done->Run();
}
