#ifndef NET2_PROTOBUF_CONNECTION_HPP_
#define NET2_PROTOBUF_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/connection.hpp"
#include <boost/function.hpp>
#include <glog/logging.h>
#include <protobuf/message.h>
#include <protobuf/descriptor.h>
#include <protobuf/service.h>
#include <boost/functional/hash.hpp>
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
  void Encode(const google::protobuf::Message *msg) {
    const string &msg_name = msg->GetDescriptor()->full_name();
    header_.append(boost::lexical_cast<string>(msg_name.size()));
    header_.append(":");
    header_.append(msg_name);
    msg->AppendToString(&body_);
    header_.append(boost::lexical_cast<string>(body_.size()));
    header_.append(":");
    buffers_.push_back(boost::asio::const_buffer(header_.c_str(), header_.size()));
    buffers_.push_back(boost::asio::const_buffer(body_.c_str(), body_.size()));
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
  string body_length_store;
  int body_length;
  Buffer<char> body_store;
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
  ProtobufReply() : status_(IDLE) {
  }
  vector<boost::asio::const_buffer> ToBuffers() {
    vector<boost::asio::const_buffer> ret;
    return ret;
  }
  enum Status {
    IDLE,
    RUNNING,
    TERMINATED,
  };
  enum ReplyStatus {
    BAD_REQUEST,
  };

  Status status() const {
    return status_;
  }

  bool IsRunning() const {
    return status_ == RUNNING;
  }

  static ProtobufReply StockReply(ReplyStatus status) {
    return ProtobufReply();
  }
 private:
  Status status_;
};

class ProtobufRequestHandler : private boost::noncopyable {
 public:
  void HandleRequest(const ProtobufRequest &request, ProtobufReply *reply) {
    StringPiece s(request.body_store.content(), request.body_store.capacity());
    VLOG(2) << "Handle request: " << s;
  }
};

class ProtobufConnection : public ConnectionImpl<
  ProtobufRequest, ProtobufRequestHandler, ProtobufRequestParser,
  ProtobufReply> {
 public:
  typedef boost::function1<void, const google::protobuf::Message *> Listener;
  typedef boost::function1<void, const ProtobufRequest*> RequestHandler;
  bool HandleService(google::protobuf::Service *service,
                      const google::protobuf::MethodDescriptor *method,
                      const google::protobuf::Message *request_prototype,
                      const google::protobuf::Message *response_prototype,
                      const ProtobufRequest *protobuf_request) {
    google::protobuf::Message *request = request_prototype->New();
    if (!request->ParseFromArray(
        protobuf_request->body_store.content(),
        protobuf_request->body_store.capacity())) {
      LOG(WARNING) << string(protobuf_request->name_store.data(),
                             protobuf_request->name_store.size()) << " invalid format";
      return false;
    }
    google::protobuf::Message *response = response_prototype->New();
    google::protobuf::Closure *done = google::protobuf::NewCallback(
        this,
        &ProtobufConnection::CallMethodDone,
        request, response);
    service->CallMethod(method, NULL, request, response, done);
    return true;
  }
  void RegisterService(google::protobuf::Service *service) {
    const google::protobuf::ServiceDescriptor *service_descriptor =
      service->GetDescriptor();
    for (int i = 0; i < service_descriptor->method_count(); ++i) {
      const google::protobuf::MethodDescriptor *method = service_descriptor->method(i);
      const google::protobuf::Message *request = &service->GetRequestPrototype(method);
      const google::protobuf::Message *response = &service->GetResponsePrototype(method);
      RequestHandler handler = boost::bind(
          &ProtobufConnection::HandleService, this,
          service,
          method,
          request, response, _1);
      service_table_[method->full_name()] = handler;
    }
  }
  template <class CL, class MessageType>
  void RegisterListener(CL *ptr, void (CL::*member)(const MessageType *)) {
  }
 private:
  void CallMethodDone(google::protobuf::Message *request,
                      google::protobuf::Message *response) {
    delete request;
    delete response;
  }
  typedef hash_map<string, RequestHandler> RequestHandlerTable;
  RequestHandlerTable service_table_;
};
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
