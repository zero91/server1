#include "server/protobuf_connection.hpp"
ProtobufLineFormatParser::ProtobufLineFormatParser()
  : state_(Start) {
}

boost::tribool ProtobufLineFormatParser::Consume(
    ProtobufLineFormat* req, char input) {
  switch (state_) {
    case End:
    case Start:
      {
        if (!isdigit(input)) {
          return false;
        }
        state_ = NameLength;
        req->name_length_store.clear();
        req->name_length_store.push_back(input);
        return boost::indeterminate;
      }
    case NameLength:
      if (input == ':') {
        state_ = Name;
        req->name_length = boost::lexical_cast<int>(req->name_length_store);
        req->name_store.reserve(req->name_length);
        return boost::indeterminate;
      } else if (!isdigit(input)) {
        return false;
      } else {
        req->name_length_store.push_back(input);
        return boost::indeterminate;
      }
    case Name:
      if (req->name_store.full()) {
        return false;
      }
      req->name_store.push_back(input);
      if (req->name_store.full()) {
        req->name.assign(req->name_store.content(),
                         req->name_store.capacity());
        req->body_length_store.clear();
        state_ = BodyLength;
      }
      return boost::indeterminate;
    case BodyLength:
      if (input == ':') {
        state_ = Body;
        req->body_length = boost::lexical_cast<int>(req->body_length_store);
        req->body_store.reserve(req->body_length);
        return boost::indeterminate;
      } else if (!isdigit(input)) {
        return false;
      } else {
        req->body_length_store.push_back(input);
        return boost::indeterminate;
      }
    case Body:
      if (req->body_store.full()) {
        return false;
      }
      req->body_store.push_back(input);
      if (req->body_store.full()) {
        req->body.assign(req->body_store.content(),
                         req->body_store.capacity());
        state_ = End;
        return true;
      }
      return boost::indeterminate;
    default:
      return false;
  }
}
bool ProtobufHandler::HandleService(
    google::protobuf::Service *service,
    const google::protobuf::MethodDescriptor *method,
    const google::protobuf::Message *request_prototype,
    const google::protobuf::Message *response_prototype,
    const ProtobufLineFormat &protobuf_request,
    FullDualChannel *connection,
    ProtobufReply *reply) {
  VLOG(2) << "Handler service: " << method->full_name();
  google::protobuf::Message *request = request_prototype->New();
  if (!request->ParseFromArray(
      protobuf_request.body.c_str(),
      protobuf_request.body.size())) {
    LOG(WARNING) << protobuf_request.name << " invalid format";
    return false;
  }
  google::protobuf::Message *response = response_prototype->New();

  google::protobuf::Closure *done = NewClosure(
      boost::bind(&ProtobufHandler::CallServiceMethodDone,
                  this,
                  request,
                  response,
                  reply));
  service->CallMethod(method, connection, request, response, done);
  return true;
}

void ProtobufHandler::CallServiceMethodDone(
    google::protobuf::Message *request,
    google::protobuf::Message *response,
    ProtobufReply *reply) {
  reply->PushMessage(shared_ptr<google::protobuf::Message>(response));
  VLOG(3) << "CallServiceMethodDone()";
  delete request;
}

void ProtobufHandler::RegisterService(
    google::protobuf::Service *service) {
  const google::protobuf::ServiceDescriptor *service_descriptor =
    service->GetDescriptor();
  for (int i = 0; i < service_descriptor->method_count(); ++i) {
    const google::protobuf::MethodDescriptor *method = service_descriptor->method(i);
    const google::protobuf::Message *request = &service->GetRequestPrototype(method);
    const google::protobuf::Message *response = &service->GetResponsePrototype(method);
    ProtobufLineFormatHandler handler = boost::bind(
        &ProtobufHandler::HandleService, this,
        service,
        method,
        request, response, _1, _2, _3);
    handler_table_->insert(make_pair(method->full_name(), handler));
  }
}

void ProtobufHandler::HandleLineFormat(
    const ProtobufLineFormat &request,
    FullDualChannel *connection,
    ProtobufReply *reply) {
  VLOG(2) << "Handle request: " << request.name << " Handler table size: "
          << handler_table_->size();
  HandlerTable::const_iterator it = handler_table_->find(
      request.name);
  if (it == handler_table_->end()) {
    VLOG(2) << "Unknown request";
    reply->set_reply_status(ProtobufReply::UNKNOWN_REQUEST);
    return;
  }
  it->second(request, connection, reply);
}

bool ProtobufHandler::PushResponseCallback(
      const string &name,
      const ProtobufResponseQueue::ResponseHandler &call) {
  VLOG(2) << "Push response: " << name;
  ResponseHandlerTable::iterator it = response_handler_table_->find(name);
  if (it == response_handler_table_->end()) {
    shared_ptr<ProtobufResponseQueue> response_queue(new ProtobufResponseQueue);
    ProtobufLineFormatHandler response_handler = boost::bind(
        &ProtobufResponseQueue::HandleResponse,
        response_queue->shared_from_this(),
        _1, _2, _3);
    response_handler_table_->insert(make_pair(name, response_queue));
    VLOG(2) << "response_name: " << name;
    if (handler_table_->find(name) == handler_table_->end()) {
      handler_table_->insert(make_pair(name, response_handler));
    }
    VLOG(2) << "Add Unknown response type: " << name << " table size: " <<
      response_handler_table_->size();
    it = response_handler_table_->find(name);
  }
  it->second->PushResponseCallback(call);
  return true;
}

void ProtobufConnection::CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done) {
  reply_.PushMessage(method->full_name(), request);
  handler_.PushResponseCallback(
      response->GetDescriptor()->full_name(),
      boost::bind(&ProtobufConnection::CallMethodCallback, this,
                  _1, controller, response, done));
  ScheduleWrite();
  ScheduleRead();
}

void ProtobufConnection::CallMethodCallback(
    const ProtobufLineFormat &line_format,
    google::protobuf::RpcController *controller,
    google::protobuf::Message *response,
    google::protobuf::Closure *done) {
  VLOG(3) << "Handle response message "
          << response->GetDescriptor()->full_name()
          << " content: " << line_format.body
          << " size: " << line_format.body.size();
  if (!response->ParseFromArray(line_format.body.c_str(),
                                line_format.body.size())) {
    LOG(WARNING) << "Fail to parse the response, name:"
                 << line_format.name;
    controller->SetFailed("Fail to parse the response, name:" +
                          line_format.name);
    if (done) done->Run();
    return;
  }
  if (done) done->Run();
}
