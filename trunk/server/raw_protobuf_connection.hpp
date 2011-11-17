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



#ifndef NET2_RAW_PROTOBUF_CONNECTION_HPP_
#define NET2_RAW_PROTOBUF_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/connection.hpp"
#include <server/meta.pb.h>
#include <boost/function.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <glog/logging.h>
#include <protobuf/message.h>
#include <protobuf/descriptor.h>
#include <protobuf/service.h>
#include "thread/notifier.hpp"
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

class ProtobufDecoder {
 public:
  /// Construct ready to parse the request method.
  ProtobufDecoder() : state_(Start) {
  };

  /// Parse some data. The boost::tribool return value is true when a complete request
  /// has been parsed, false if the data is invalid, indeterminate when more
  /// data is required. The InputIterator return value indicates how much of the
  /// input has been consumed.
  template <typename InputIterator>
  boost::tuple<boost::tribool, InputIterator> Decode(
      InputIterator begin, InputIterator end) {
    VLOG(2) << "Decode size: " << (end - begin);
    while (begin != end) {
      boost::tribool result = Consume(*begin++);
      if (result || !result) {
        return boost::make_tuple(result, begin);
      }
    }
    boost::tribool result = boost::indeterminate;
    return boost::make_tuple(result, begin);
  }

  const ProtobufLineFormat::MetaData &meta() const {
    return meta_;
  }
  void reset() {
    state_ = Start;
    length_store_.clear();
    length_ = 0;
    content_.clear();
  }
private:
  /// Handle the next character of input.
  boost::tribool Consume(char input);

  /// The current state of the parser.
  enum State {
    Start,
    Length,
    Content,
    End
  } state_;

  string length_store_;
  int length_;
  Buffer<char> content_;
  ProtobufLineFormat::MetaData meta_;
};

class RawProtobufConnection : public RawConnectionImpl<ProtobufDecoder> {
 private:
  typedef boost::function2<void,
          const ProtobufDecoder*,
          boost::shared_ptr<Connection> > HandlerFunctor;
  typedef hash_map<uint64, HandlerFunctor> HandlerTable;
 public:
  RawProtobufConnection(
      const string &name,
      boost::shared_ptr<Connection> connection,
      ProtobufConnection *service_connection);
  ~RawProtobufConnection();

  // Thread safe.
  void CallMethod(RawConnection::StatusPtr status,
                  const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done);
 private:
  void ReleaseResponseTable();
  virtual bool Handle(const ProtobufDecoder *decoder);
  // The response handler table is per connection.
  HandlerTable response_handler_table_;
  boost::mutex response_handler_table_mutex_;
  ProtobufConnection *service_connection_;
};
#endif  // NET2_RAW_PROTOBUF_CONNECTION_HPP_
