#ifndef NET2_PROTOBUF_CONNECTION_HPP_
#define NET2_PROTOBUF_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/connection.hpp"
#include <boost/function.hpp>
#include <glog/logging.h>
#include <protobuf/message.h>
#include <protobuf/descriptor.h>
#include <protobuf/service.h>
// Encoder the Protobuf to line format.
// The line format is:
// name_length:name body_length:body
class ProtobufEncoder {
 public:
  void Init() {
    header_.clear();
    body_.clear();
    buffers_.clear();
  }
  bool Encode(const google::protobuf::Message *msg) {
    if (!msg->AppendToString(&body_)) {
      return false;
    }
    if (body_.empty()) {
      return false;
    }
    const string &msg_name = msg->GetDescriptor()->full_name();
    header_.append(boost::lexical_cast<string>(msg_name.size()));
    header_.append(":");
    header_.append(msg_name);
    header_.append(boost::lexical_cast<string>(body_.size()));
    header_.append(":");
    buffers_.push_back(boost::asio::const_buffer(header_.c_str(), header_.size()));
    buffers_.push_back(boost::asio::const_buffer(body_.c_str(), body_.size()));
    return true;
  }
  const vector<boost::asio::const_buffer> &ToBuffers() const {
    return buffers_;
  }
 private:
  string header_, body_;
  vector<boost::asio::const_buffer> buffers_;
};

struct ProtobufRequest {
  string name_length_store;
  int name_length;
  Buffer<char> name_store;
  string name;
  string body_length_store;
  int body_length;
  Buffer<char> body_store;
  string body;
};

class ProtobufRequestParser {
 public:
  /// Construct ready to parse the request method.
  ProtobufRequestParser();

  /// Reset to initial parser state.
  void reset();

  /// Parse some data. The boost::tribool return value is true when a complete request
  /// has been parsed, false if the data is invalid, indeterminate when more
  /// data is required. The InputIterator return value indicates how much of the
  /// input has been consumed.
  template <typename InputIterator>
  boost::tuple<boost::tribool, InputIterator> Parse(ProtobufRequest *req,
      InputIterator begin, InputIterator end) {
    while (begin != end) {
      boost::tribool result = Consume(req, *begin++);
      if (result || !result) {
        return boost::make_tuple(result, begin);
      }
    }
    boost::tribool result = boost::indeterminate;
    return boost::make_tuple(result, begin);
  }
private:
  /// Handle the next character of input.
  boost::tribool Consume(ProtobufRequest *req, char input);

  /// The current state of the parser.
  enum State {
    Start,
    NameLength,
    Name,
    BodyLength,
    Body,
    End,
  } state_;
};

class ProtobufReply {
 public:
  enum Status {
    IDLE,
    RUNNING,
    TERMINATED,
  };
  enum ReplyStatus {
    SUCCEED_WITHOUT_CONTENT,
    SUCCEED_WITH_CONTENT,
    BAD_REQUEST,
    UNKNOWN_REQUEST,
  };
  ProtobufReply() : status_(IDLE),
                    reply_status_(SUCCEED_WITHOUT_CONTENT) {
  }
  vector<boost::asio::const_buffer> ToBuffers() {
    return vector<boost::asio::const_buffer> ();
  }

  ReplyStatus reply_status() const {
    return reply_status_;
  }

  void set_reply_status(ReplyStatus reply_status) {
    reply_status_ = reply_status;
  }

  void set_status(Status status) {
    status_ = status;
  }

  Status status() const {
    return status_;
  }
  bool Encode() {
    encoder_.Init();
    return encoder_.Encode(response_message_.get());
  }

  bool IsRunning() const {
    return status_ == RUNNING;
  }

  void set_request_message(google::protobuf::Message *msg) {
    request_message_.reset(msg);
  }

  void set_response_message(google::protobuf::Message *msg) {
    response_message_.reset(msg);
  }
 private:
  scoped_ptr<google::protobuf::Message> request_message_;
  scoped_ptr<google::protobuf::Message> response_message_;
  ProtobufEncoder encoder_;
  Status status_;
  ReplyStatus reply_status_;
};

class ProtobufRequestHandler : private boost::noncopyable {
 public:
  typedef boost::function2<
    void, const ProtobufRequest&, ProtobufReply*> RequestHandler;
  bool HandleService(google::protobuf::Service *service,
                     const google::protobuf::MethodDescriptor *method,
                     const google::protobuf::Message *request_prototype,
                     const google::protobuf::Message *response_prototype,
                     const ProtobufRequest &protobuf_request,
                     ProtobufReply *reply);
  void HandleRequest(const ProtobufRequest &request, ProtobufReply *reply);
  void RegisterService(google::protobuf::Service *service);
 private:
  void CallServiceMethodDone(ProtobufReply *reply);
  typedef hash_map<string, RequestHandler> RequestHandlerTable;
  RequestHandlerTable handler_table_;
};

class ProtobufConnection : public ConnectionImpl<
  ProtobufRequest, ProtobufRequestHandler, ProtobufRequestParser,
  ProtobufReply> {
 public:
  void RegisterService(google::protobuf::Service *service) {
    request_handler_.RegisterService(service);
  }
  template <class CL, class MessageType>
  void RegisterListener(CL *ptr, void (CL::*member)(const MessageType *)) {
  }
};
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
