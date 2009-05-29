// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#include "server/protobuf_connection.hpp"
#include "server/protobuf_connection-inl.hpp"
// Encoder the Protobuf to line format.
// The line format is:
// length:content
typedef pair<const string *, const string *> EncodeData;
inline EncodeData EncodeMessage(const google::protobuf::Message *msg) {
  string *content = new string;
  if (!msg->AppendToString(content)) {
    delete content;
    return make_pair(static_cast<const string *>(NULL),
                     static_cast<const string *>(NULL));
  }
  string *header = new string(boost::lexical_cast<string>(content->size()));
  header->push_back(':');
  VLOG(2) << "Encode Message, header: " << *header
          << " content size: " << content->size();
  return make_pair(header, content);
};
ProtobufConnectionImpl::~ProtobufConnectionImpl() {
  VLOG(2) << name() << " : " << "Distroy protobuf connection" << this;
  boost::shared_ptr<ProtobufDecoder> decoder;
  int i = 0;
  for (HandlerTable::iterator it = response_handler_table_.begin();
       it != response_handler_table_.end(); ++it) {
    LOG(WARNING) << name() << " : " << "Call response handler " << it->first<< " in destructor NO " << ++i;
    it->second(decoder, this);
  }
}

template <>
template <>
void ConnectionImpl<ProtobufDecoder>::InternalPushData<EncodeData>(
    const EncodeData &data) {
  if (data.first == NULL) {
    LOG(WARNING) << "Push NULL data!";
    return;
  }
  incoming()->push(data.first);
  incoming()->push(data.second);
}

boost::tribool ProtobufDecoder::Consume(char input) {
  switch (state_) {
    case End:
    case Start:
      {
        if (!isdigit(input)) {
          LOG(WARNING) << "Start but is not digit";
          return false;
        }
        state_ = Length;
        length_store_.clear();
        length_store_.push_back(input);
        return boost::indeterminate;
      }
    case Length:
      if (input == ':') {
        state_ = Content;
        length_ = boost::lexical_cast<int>(length_store_);
        content_.reserve(length_);
        return boost::indeterminate;
      } else if (!isdigit(input)) {
        LOG(WARNING) << "Length is not digit";
        return false;
      } else {
        length_store_.push_back(input);
        return boost::indeterminate;
      }
    case Content:
      if (content_.full()) {
        return false;
      }
      content_.push_back(input);
      if (content_.full()) {
        if (!meta_.ParseFromArray(content_.content(),
                                  content_.capacity())) {
          LOG(WARNING) << "Parse content error";
          return false;
        }
        if (meta_.type() == ProtobufLineFormat::MetaData::REQUEST &&
            !meta_.has_response_identify()) {
          LOG(WARNING) << "request meta data should have response identify field";
          return false;
        }
        if (meta_.content().empty()) {
          LOG(WARNING) << "Meta without content: " << meta_.DebugString();
          return false;
        }
        state_ = End;
        return true;
      }
      return boost::indeterminate;
    default:
      LOG(WARNING) << "Unknown status of ProtobufDecoder";
      return false;
  }
}

static void CallServiceMethodDone(
    ProtobufConnectionImpl *connection,
    boost::shared_ptr<const ProtobufDecoder> decoder,
    boost::shared_ptr<google::protobuf::Message> resource,
    boost::shared_ptr<google::protobuf::Message> response) {
  VLOG(2) << connection->name() << " : " << "HandleService->CallServiceMethodDone()";
  CHECK(decoder.get() != NULL);
  const ProtobufLineFormat::MetaData &request_meta = decoder->meta();
  ProtobufLineFormat::MetaData response_meta;
  response_meta.set_type(ProtobufLineFormat::MetaData::RESPONSE);
  response_meta.set_identify(request_meta.response_identify());
  CHECK(response->AppendToString(response_meta.mutable_content()))
    << "Fail to serialize response for requst: " << request_meta.DebugString();
  connection->PushData(EncodeMessage(&response_meta));
  connection->ScheduleWrite();
}

static void HandleService(
    google::protobuf::Service *service,
    const google::protobuf::MethodDescriptor *method,
    const google::protobuf::Message *request_prototype,
    const google::protobuf::Message *response_prototype,
    boost::shared_ptr<const ProtobufDecoder> decoder,
    ProtobufConnectionImpl *connection) {
  VLOG(2) << connection->name() << " : " <<  "HandleService: " << method->full_name();
  const ProtobufLineFormat::MetaData &meta = decoder->meta();
  boost::shared_ptr<google::protobuf::Message> request(request_prototype->New());
  const string &content = decoder->meta().content();
  VLOG(2) << connection->name() << " : " << "content size: " << content.size();
  if (!request->ParseFromArray(
      content.c_str(),
      content.size())) {
    LOG(WARNING) << meta.DebugString() << " invalid format";
    return;
  }
  boost::shared_ptr<google::protobuf::Message> response(response_prototype->New());
  google::protobuf::Closure *done = NewClosure(
      boost::bind(CallServiceMethodDone,
                  connection,
                  decoder,
                  request,
                  response));
  service->CallMethod(method, connection, request.get(), response.get(), done);
}

bool ProtobufConnectionImpl::RegisterService(google::protobuf::Service *service) {
  if (this->IsConnected()) {
    LOG(WARNING) << "Can't register service to running connection.";
    return false;
  }
  const google::protobuf::ServiceDescriptor *service_descriptor =
    service->GetDescriptor();
  for (int i = 0; i < service_descriptor->method_count(); ++i) {
    const google::protobuf::MethodDescriptor *method = service_descriptor->method(i);
    const google::protobuf::Message *request = &service->GetRequestPrototype(method);
    const google::protobuf::Message *response = &service->GetResponsePrototype(method);
    const string &method_name = method->full_name();
    const uint64 method_fingerprint = hash8(method_name);
    HandlerTable::const_iterator it = handler_table_->find(method_fingerprint);
    CHECK(it == handler_table_->end())
      << " unfortunately, the method name: " << method_name
      << " is conflict with another name after hash("
      << method_fingerprint << ") please change.";
    handler_table_->insert(make_pair(
        method_fingerprint,
        boost::bind(
        HandleService, service, method, request, response,
        _1, _2)));
  }
}

void ProtobufConnectionImpl::Handle(boost::shared_ptr<const ProtobufDecoder> decoder) {
  const ProtobufLineFormat::MetaData &meta = decoder->meta();
  HandlerTable::value_type::second_type handler;
  HandlerTable::iterator it = handler_table_->find(meta.identify());
  if (it != handler_table_->end()) {
    handler = it->second;
  } else {
    boost::mutex::scoped_lock locker(response_handler_table_mutex_);
    it = response_handler_table_.find(meta.identify());
    if (it == response_handler_table_.end()) {
      VLOG(2) << name() << " : " << "Unknown request" << meta.DebugString();
      return;
    }
    handler = it->second;
    response_handler_table_.erase(it);
    VLOG(2) << name() << " Remove: " << it->first << " from response handler table, size: " << response_handler_table_.size();
  }
  handler(decoder, this);
}

ProtobufConnectionImpl* ProtobufConnectionImpl::Clone() {
  static int i = 0;
  ProtobufConnectionImpl* connection = new ProtobufConnectionImpl(this->timeout_ms_);
  connection->handler_table_ = this->handler_table_;
  connection->set_name(this->name() + boost::lexical_cast<string>(i++));
  VLOG(2) << "Clone protobufconnection: " << name() << " -> " << connection->name();
  return connection;
}

static void CallMethodCallback(
    boost::shared_ptr<const ProtobufDecoder> decoder,
    ProtobufConnectionImpl *connection,
    google::protobuf::RpcController *controller,
    google::protobuf::Message *response,
    google::protobuf::Closure *done) {
  RpcController *rpc_controller = dynamic_cast<RpcController*>(
      controller);
  if (decoder.get() == NULL) {
    VLOG(2) << "NULL Decoder, may call from destructor";
    if (rpc_controller) rpc_controller->Notify();
    if (done) done->Run();
    return;
  }
  const ProtobufLineFormat::MetaData &meta = decoder->meta();
  VLOG(3) << "Handle response message "
          << response->GetDescriptor()->full_name()
          << " response: " << meta.DebugString();
  if (!response->ParseFromArray(meta.content().c_str(),
                                meta.content().size())) {
    LOG(WARNING) << "Fail to parse the response :"
                 << meta.DebugString();
    controller->SetFailed("Fail to parse the response:" + meta.DebugString());
    if (rpc_controller) rpc_controller->Notify();
    if (done) done->Run();
    return;
  }
  if (rpc_controller) rpc_controller->Notify();
  if (done) done->Run();
}

void ProtobufConnectionImpl::Timeout(const boost::system::error_code& e,
                                 uint64 response_identify,
                                 google::protobuf::RpcController *controller,
                                 google::protobuf::Closure *done,
                                 boost::shared_ptr<boost::asio::deadline_timer> timer) {
  LOG(INFO ) << "Timeout";
  if (e) {
    LOG(WARNING) << "Timeout error: " << e.message();
    return;
  }
  RpcController *rpc_controller = dynamic_cast<RpcController*>(
      controller);
  {
    boost::mutex::scoped_lock locker(response_handler_table_mutex_);
    HandlerTable::iterator it = response_handler_table_.find(response_identify);
    if (it == response_handler_table_.end()) {
      VLOG(2) << name() << " : "
              << response_identify << " timer expire";
      return;
    }
    VLOG(2) << name() << " : " << response_identify << " timeout";
    response_handler_table_.erase(it);
  }
  controller->SetFailed("Timeout");
  if (rpc_controller) rpc_controller->Notify();
  if (done) done->Run();
}

void ProtobufConnectionImpl::CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done) {
  VLOG(2) << name() << " : " << "Call Method connection";
  uint64 request_identify = hash8(method->full_name());
  uint64 response_identify = hash8(response->GetDescriptor()->full_name());
  ProtobufLineFormat::MetaData meta;
  {
    boost::mutex::scoped_lock locker(response_handler_table_mutex_);
    HandlerTable::const_iterator it = response_handler_table_.find(response_identify);
    while (it != response_handler_table_.end()) {
      static int seq = 1;
      ++seq;
      response_identify += seq;
      it = response_handler_table_.find(response_identify);
    }
    meta.set_identify(request_identify);
    meta.set_type(ProtobufLineFormat::MetaData::REQUEST);
    meta.set_response_identify(response_identify);
    if (!request->AppendToString(meta.mutable_content())) {
      LOG(WARNING) << "Fail to serialze request form method: "
        << method->full_name();
    }
    response_handler_table_.insert(make_pair(
        response_identify,
        boost::bind(CallMethodCallback, _1, _2, controller, response, done)));
    VLOG(2) << name() << " Insert: " << response_identify << " to response handler table, size: " << response_handler_table_.size();
  }
  PushData(EncodeMessage(&meta));
  ScheduleWrite();
  ScheduleRead();
  if (timeout_ms_ > 0) {
    boost::shared_ptr<boost::asio::deadline_timer> timer(
        new boost::asio::deadline_timer(socket_->get_io_service()));
    timer->expires_from_now(boost::posix_time::milliseconds(timeout_ms_));
    const boost::function1<void, const boost::system::error_code&> h =
          boost::bind(&ProtobufConnectionImpl::Timeout, this, _1, response_identify, controller, done, timer);
    timer->async_wait(h);
  }
}

ProtobufConnection::ProtobufConnection(int timeout) : Connection(new ProtobufConnectionImpl(timeout)) {
  VLOG(2) << "New protobuf connection" << this << " timeout: " << timeout;
}

ProtobufConnection::ProtobufConnection() : Connection(
  new ProtobufConnectionImpl()) {
}

bool ProtobufConnection::RegisterService(google::protobuf::Service *service) {
  return dynamic_cast<ProtobufConnectionImpl*>(impl_.get())->RegisterService(
      service);
}

void ProtobufConnection::CallMethod(const google::protobuf::MethodDescriptor *method,
                                    google::protobuf::RpcController *controller,
                                    const google::protobuf::Message *request,
                                    google::protobuf::Message *response,
                                    google::protobuf::Closure *done) {
  dynamic_cast<ProtobufConnectionImpl*>(impl_.get())->CallMethod(
      method,
      controller,
      request,
      response,
      done);
}
