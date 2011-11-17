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



#include "server/connection.hpp"
#include "server/raw_connection.hpp"
#include "server/protobuf_connection.hpp"
#include "server/raw_protobuf_connection.hpp"
#define RawConnTrace VLOG(2) << name() << ".protobuf : " << __func__ << " "
RawProtobufConnection::~RawProtobufConnection() {
  VLOG(2) << name() << " : " << "Distroy protobuf connection";
  ReleaseResponseTable();
}

void RawProtobufConnection::ReleaseResponseTable() {
  vector<HandlerFunctor> handlers;
  {
    boost::mutex::scoped_lock locker(response_handler_table_mutex_);
    for (HandlerTable::iterator it = response_handler_table_.begin();
         it != response_handler_table_.end(); ++it) {
      handlers.push_back(it->second);
    }
    response_handler_table_.clear();
  }
  for (int i = 0; i < handlers.size(); ++i) {
    LOG(WARNING) << name() << " : " << "Call response handler in ReleaseResponseTable NO " << i;
    handlers[i](NULL, connection_);
  }
  handlers.clear();
}

template <>
void RawConnection::InternalPushData<EncodeData>(
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
        VLOG(2) << "Length: " << length_store_ << " length size: " << length_store_.size() << " Content size: " << length_;
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
          LOG(WARNING) << "Meta without content: ";
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

bool RawProtobufConnection::Handle(
    const ProtobufDecoder *decoder) {
  CHECK(decoder != NULL);
  bool ret = service_connection_->Handle(
      connection_,
      decoder);
  if (ret) {
    RawConnTrace << "Client->Servive ";
    return true;
  }
  const ProtobufLineFormat::MetaData &meta = decoder->meta();
  HandlerTable::value_type::second_type handler;
  {
    boost::mutex::scoped_lock locker(response_handler_table_mutex_);
    HandlerTable::iterator it = response_handler_table_.find(meta.identify());
    if (it == response_handler_table_.end()) {
      VLOG(2) << name() << " : " << "Unknown request";
      return false;
    }
    handler = it->second;
    response_handler_table_.erase(it);
    RawConnTrace << "Remove: " << meta.identify() << " table size: " << response_handler_table_.size();
  }
  handler(decoder, connection_);
  return true;
}

RawProtobufConnection::RawProtobufConnection(
    const string &name,
    boost::shared_ptr<Connection> connection,
    ProtobufConnection *service_connection)
  : RawConnectionImpl<ProtobufDecoder>(name, connection),
    service_connection_(service_connection) {
}

static void CallMethodCallback(
    const ProtobufDecoder *decoder,
    boost::shared_ptr<Connection> connection,
    google::protobuf::RpcController *controller,
    google::protobuf::Message *response,
    google::protobuf::Closure *done) {
  ScopedClosure run(done);
  RpcController *rpc_controller = dynamic_cast<RpcController*>(
      controller);
  if (decoder == NULL) {
    VLOG(2) << "NULL Decoder, may call from destructor";
    if (rpc_controller) {
      rpc_controller->SetFailed("Abort");
      rpc_controller->Notify();
    }
    return;
  }
  const ProtobufLineFormat::MetaData &meta = decoder->meta();
  VLOG(2) << connection->name() << " : " << "Handle response message "
          << response->GetDescriptor()->full_name()
          << " identify: " << meta.identify();
  if (!response->ParseFromArray(meta.content().c_str(),
                                            meta.content().size())) {
    LOG(WARNING) << connection->name() << " : " << "Fail to parse the response :";
    if (rpc_controller) {
      rpc_controller->SetFailed("Fail to parse the response:");
      rpc_controller->Notify();
      return;
    }
  }
  if (rpc_controller) rpc_controller->Notify();
}

void RawProtobufConnection::CallMethod(
    RawConnection::StatusPtr status,
    const google::protobuf::MethodDescriptor *method,
    google::protobuf::RpcController *controller,
    const google::protobuf::Message *request,
    google::protobuf::Message *response,
    google::protobuf::Closure *done) {
  VLOG(2) << name() << " : " << "CallMethod";
  uint64 request_identify = hash8(method->full_name());
  uint64 response_identify = hash8(response->GetDescriptor()->full_name());
  ProtobufLineFormat::MetaData meta;
  RpcController *rpc_controller = dynamic_cast<RpcController*>(
      controller);
  bool error = false;
  string reason;
  EncodeData data;
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
    response_handler_table_.insert(make_pair(
        response_identify,
        boost::bind(CallMethodCallback, _1, _2, controller, response, done)));
    error =  !request->AppendToString(meta.mutable_content());
    if (error) {
      LOG(WARNING) << name() << " : "
           << "Fail to serialze request form method: "
           << method->full_name();
      reason = "AppendTostringError";
      goto failed;
    }
    VLOG(2) << name() << " Insert: "
            << response_identify << " to response handler table, size: "
            << response_handler_table_.size();
  }
  data = EncodeMessage(&meta);
  error = !PushData(data);
  if (error) {
    LOG(WARNING) << name() << " : " << "PushData error, connection may closed";
    reason = "PushDataError";
    delete data.first;
    delete data.second;
    goto failed;
  }
  RawConnTrace << " PushData, " << " incoming: " << incoming()->size();
  error = !ScheduleWrite(status);
  if (error) {
    LOG(WARNING) << name() << " : "
        << "ScheduleWrite error, connection may closed";
    reason = "ScheduleWriteError";
    delete data.first;
    delete data.second;
    goto failed;
  }
  return;
failed:
  {
    boost::mutex::scoped_lock locker(response_handler_table_mutex_);
    response_handler_table_.erase(response_identify);
  }
  if (rpc_controller) {
    rpc_controller->SetFailed(reason);
    rpc_controller->Notify();
  }
  if (done) {
    done->Run();
  }
}
#undef RawConnTrace
