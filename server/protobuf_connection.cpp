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



#include "server/protobuf_connection.hpp"
#include "server/raw_protobuf_connection.hpp"
static void CallServiceMethodDone(
    boost::shared_ptr<Connection> connection,
    const ProtobufDecoder *decoder,
    boost::shared_ptr<google::protobuf::Message> resource,
    boost::shared_ptr<google::protobuf::Message> response) {
  VLOG(2) << connection->name() << " : " << "HandleService->CallServiceMethodDone()";
  CHECK(decoder != NULL);
  const ProtobufLineFormat::MetaData &request_meta = decoder->meta();
  ProtobufLineFormat::MetaData response_meta;
  response_meta.set_type(ProtobufLineFormat::MetaData::RESPONSE);
  response_meta.set_identify(request_meta.response_identify());
  CHECK(response->AppendToString(response_meta.mutable_content()))
    << "Fail to serialize response for requst: ";
  EncodeData data = EncodeMessage(&response_meta);
  if (!connection->PushData(data)) {
    delete data.first;
    delete data.second;
  }
  if (!connection->ScheduleWrite()) {
    delete data.first;
    delete data.second;
  }
}

static void HandleService(
    google::protobuf::Service *service,
    const google::protobuf::MethodDescriptor *method,
    const google::protobuf::Message *request_prototype,
    const google::protobuf::Message *response_prototype,
    const ProtobufDecoder *decoder,
    boost::shared_ptr<Connection> connection) {
  VLOG(2) << connection->name() << " : " <<  "HandleService: " << method->full_name();
  const ProtobufLineFormat::MetaData &meta = decoder->meta();
  boost::shared_ptr<google::protobuf::Message> request(request_prototype->New());
  const string &content = decoder->meta().content();
  VLOG(2) << connection->name() << " : " << "content size: " << content.size();
  if (!request->ParseFromArray(
      content.c_str(),
      content.size())) {
    LOG(WARNING) << connection->name() << " : " << "HandleService but invalid format";
    return;
  }
  boost::shared_ptr<google::protobuf::Message> response(response_prototype->New());
  google::protobuf::Closure *done = NewClosure(
      boost::bind(CallServiceMethodDone,
                  connection,
                  decoder,
                  request,
                  response));
  service->CallMethod(method, connection.get(), request.get(), response.get(), done);
}

bool ProtobufConnection::RegisterService(google::protobuf::Service *service) {
  if (handler_table_.get() == NULL) {
    handler_table_.reset(new HandlerTable);
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

void ProtobufConnection::CallMethod(const google::protobuf::MethodDescriptor *method,
                                    google::protobuf::RpcController *controller,
                                    const google::protobuf::Message *request,
                                    google::protobuf::Message *response,
                                    google::protobuf::Closure *done) {
  RawConnectionStatus::Locker locker(status_->mutex());
  if (status_->closing() || impl_.get() == NULL) {
    VLOG(2) << "CallMethod " << name() << " but is closing";
    RpcController *rpc_controller = dynamic_cast<RpcController*>(
        controller);
    if (rpc_controller) {
      rpc_controller->SetFailed("Impl Connection is disconnected");
      rpc_controller->Notify();
    }
    if (done) {
      done->Run();
    }
    return;
  }
  RawProtobufConnection *impl = dynamic_cast<RawProtobufConnection*>(impl_.get());
  impl->CallMethod(status_, method, controller, request, response, done);
}

bool ProtobufConnection::Handle(
    boost::shared_ptr<Connection> connection,
    const ProtobufDecoder *decoder) const {
  if (handler_table_.get() == NULL) {
    return false;
  }
  VLOG(2) << connection->name() << ".ProtobufConnection.Handle";
  const ProtobufLineFormat::MetaData &meta = decoder->meta();
  HandlerTable::value_type::second_type handler;
  HandlerTable::const_iterator it = handler_table_->find(meta.identify());
  if (it != handler_table_->end()) {
    it->second(decoder, connection);
    return true;
  }
  return false;
}

boost::shared_ptr<Connection> ProtobufConnection::Span(
    boost::asio::ip::tcp::socket *socket) {
  boost::shared_ptr<ProtobufConnection> connection(new ProtobufConnection(name() + ".span"));
  if (!connection->Attach(this, socket)) {
    connection.reset();
  }
  return connection;
}

bool ProtobufConnection::Attach(
  ProtobufConnection *service_connection,
  boost::asio::ip::tcp::socket *socket) {
  static int i = 0;
  const string span_name = this->name() + boost::lexical_cast<string>(i++);
  RawConnection *raw_connection(new RawProtobufConnection(
      span_name,
      shared_from_this(),
      service_connection));
  if (raw_connection == NULL) {
    LOG(WARNING) << "Fail to allocate RawProtobufConnection, not enough memory?";
    delete socket;
    return false;
  }

  impl_.reset(raw_connection);
  impl_->InitSocket(status_, socket);
  return true;
}
