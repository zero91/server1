#include "client/channel.hpp"

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                         google::protobuf::RpcController *controller,
                         const google::protobuf::Message *request,
                         google::protobuf::Message *response,
                         google::protobuf::Closure *done) {
  encoder_.Init();
  if (encoder_.Encode(method->full_name(), request)) {
    controller->SetFailed("Encode failed");
    return;
  }
  if (!client_.Connect(server_, port_)) {
    controller->SetFailed("Fail to connection " +
                          server_ + " : " + port_);
    done->Run();
    return;
  }
  client_.set_listener(boost::bind(
      &RpcChannel::HandleResponse, this,
      _1,
      controller, response, done));
}

void RpcChannel::HandleResponse(
    const ProtobufLineFormat *line_format,
    google::protobuf::RpcController *controller,
    google::protobuf::Message *response,
    google::protobuf::Closure *done) {
  if (line_format == NULL) {
    LOG(WARNING) << "Reply line format is null";
    controller->SetFailed("Reply line format is null.");
    done->Run();
    return;
  }
  if (!response->ParseFromArray(line_format->body.c_str(),
                                line_format->body.size())) {
    LOG(WARNING) << "Fail to parse the response, name:"
                 << line_format->name;
    controller->SetFailed("Fail to parse the response, name:" +
                          line_format->name);
    done->Run();
    return;
  }
  done->Run();
}
