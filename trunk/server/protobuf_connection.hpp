#ifndef NET2_PROTOBUF_CONNECTION_HPP_
#define NET2_PROTOBUF_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/connection.hpp"
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <glog/logging.h>
#include <protobuf/message.h>
#include <protobuf/descriptor.h>
#include <protobuf/service.h>
typedef vector<boost::asio::const_buffer> ConstBufferVector;
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
    const string &msg_name = msg->GetDescriptor()->full_name();
    return Encode(msg_name, msg);
  }

  bool Encode(const string &name, const google::protobuf::Message *msg) {
    if (!msg->AppendToString(&body_)) {
      return false;
    }
    if (body_.empty()) {
      return false;
    }
    header_.append(boost::lexical_cast<string>(name.size()));
    header_.append(":");
    header_.append(name);
    header_.append(boost::lexical_cast<string>(body_.size()));
    header_.append(":");
    buffers_.push_back(boost::asio::const_buffer(header_.c_str(), header_.size()));
    buffers_.push_back(boost::asio::const_buffer(body_.c_str(), body_.size()));
    return true;
  }
  const ConstBufferVector &ToBuffers() const {
    return buffers_;
  }
 private:
  string header_, body_;
  ConstBufferVector buffers_;
};

struct ProtobufLineFormat {
  string name_length_store;
  int name_length;
  Buffer<char> name_store;
  string name;
  string body_length_store;
  int body_length;
  Buffer<char> body_store;
  string body;
};

class ProtobufLineFormatParser {
 public:
  /// Construct ready to parse the request method.
  ProtobufLineFormatParser();

  /// Reset to initial parser state.
  void reset();

  /// Parse some data. The boost::tribool return value is true when a complete request
  /// has been parsed, false if the data is invalid, indeterminate when more
  /// data is required. The InputIterator return value indicates how much of the
  /// input has been consumed.
  template <typename InputIterator>
  boost::tuple<boost::tribool, InputIterator> Parse(ProtobufLineFormat *req,
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
  boost::tribool Consume(ProtobufLineFormat *req, char input);

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

class ProtobufRequestHandler {
 private:
  typedef boost::function2<
    void, const ProtobufLineFormat&, ProtobufReply*> RequestHandler;
  typedef hash_map<string, RequestHandler> RequestHandlerTable;
 public:
  ProtobufRequestHandler() : handler_table_(new RequestHandlerTable) {
  }
  bool HandleService(google::protobuf::Service *service,
                     const google::protobuf::MethodDescriptor *method,
                     const google::protobuf::Message *request_prototype,
                     const google::protobuf::Message *response_prototype,
                     const ProtobufLineFormat &protobuf_request,
                     ProtobufReply *reply);
  void HandleLineFormat(const ProtobufLineFormat &request, ProtobufReply *reply);
  void RegisterService(google::protobuf::Service *service);
 private:
  void CallServiceMethodDone(ProtobufReply *reply);
  shared_ptr<RequestHandlerTable> handler_table_;
};

class ProtobufConnection : public ConnectionImpl<
  ProtobufLineFormat, ProtobufRequestHandler, ProtobufLineFormatParser,
  ProtobufReply> {
 public:
  Connection *Clone() {
    boost::mutex::scoped_lock locker(mutex_);
    return ConnectionImpl<ProtobufLineFormat,
           ProtobufRequestHandler,
           ProtobufLineFormatParser,
           ProtobufReply>::Clone();
  }
  void RegisterService(google::protobuf::Service *service) {
    boost::mutex::scoped_lock locker(mutex_);
    lineformat_handler_.RegisterService(service);
  }
  template <class CL, class MessageType>
  void RegisterListener(CL *ptr, void (CL::*member)(const MessageType *)) {
    boost::mutex::scoped_lock locker(mutex_);
  }

 private:
  boost::mutex mutex_;

};
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
