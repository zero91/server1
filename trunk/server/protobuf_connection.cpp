#include "server/protobuf_connection.hpp"
ProtobufLineFormatParser::ProtobufLineFormatParser()
  : state_(Start) {
}

void ProtobufLineFormatParser::reset() {
  state_ = Start;
}

boost::tribool ProtobufLineFormatParser::Consume(
    ProtobufLineFormat* req, char input) {
  switch (state_) {
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
bool ProtobufRequestHandler::HandleService(
    google::protobuf::Service *service,
    const google::protobuf::MethodDescriptor *method,
    const google::protobuf::Message *request_prototype,
    const google::protobuf::Message *response_prototype,
    const ProtobufLineFormat &protobuf_request,
    ProtobufReply *reply) {
  google::protobuf::Message *request = request_prototype->New();
  if (!request->ParseFromArray(
      protobuf_request.body.c_str(),
      protobuf_request.body.size())) {
    LOG(WARNING) << protobuf_request.name << " invalid format";
    return false;
  }
  google::protobuf::Message *response = response_prototype->New();
  google::protobuf::Closure *done = google::protobuf::NewCallback(
      this,
      &ProtobufRequestHandler::CallServiceMethodDone,
      boost::make_tuple(request, response, reply));
  service->CallMethod(method, NULL, request, response, done);
  return true;
}

void ProtobufRequestHandler::CallServiceMethodDone(
    boost::tuple<google::protobuf::Message *,
                 google::protobuf::Message *,
                 ProtobufReply*> tuple) {
  google::protobuf::Message *request = tuple.get<0>();
  google::protobuf::Message *response = tuple.get<1>();
  ProtobufReply *reply = tuple.get<2>();
  reply->PushMessage(shared_ptr<google::protobuf::Message>(response));
  VLOG(3) << "CallServiceMethodDone()";
  // Service is short connection.
  reply->set_status(ProtobufReply::TERMINATED);
  delete request;
}

void ProtobufRequestHandler::RegisterService(
    google::protobuf::Service *service) {
  const google::protobuf::ServiceDescriptor *service_descriptor =
    service->GetDescriptor();
  for (int i = 0; i < service_descriptor->method_count(); ++i) {
    const google::protobuf::MethodDescriptor *method = service_descriptor->method(i);
    const google::protobuf::Message *request = &service->GetRequestPrototype(method);
    const google::protobuf::Message *response = &service->GetResponsePrototype(method);
    RequestHandler handler = boost::bind(
        &ProtobufRequestHandler::HandleService, this,
        service,
        method,
        request, response, _1, _2);
    handler_table_->insert(make_pair(method->full_name(), handler));
  }
}

void ProtobufRequestHandler::HandleLineFormat(
    const ProtobufLineFormat &request, ProtobufReply *reply) {
  VLOG(2) << "Handle request: " << request.name;
  RequestHandlerTable::const_iterator it = handler_table_->find(
      request.name);
  if (it == handler_table_->end()) {
    reply->set_reply_status(ProtobufReply::UNKNOWN_REQUEST);
    return;
  }
  it->second(request, reply);
}
