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
class ProtobufEncoder : public Object {
 public:
  ProtobufEncoder(shared_ptr<google::protobuf::Message> msg)
    : shared_msg_(msg) {
    encoded_ = Encode(shared_msg_.get());
  }
  ProtobufEncoder(const string &name,
                  shared_ptr<google::protobuf::Message> msg)
    : shared_msg_(msg) {
    encoded_ = Encode(name, shared_msg_.get());
  }
  ProtobufEncoder(const google::protobuf::Message *msg)
    : msg_(msg) {
    encoded_ = Encode(msg_);
  }
  ProtobufEncoder(const string &name,
                  const google::protobuf::Message *msg)
    : msg_(msg) {
    encoded_ = Encode(name, msg_);
  }
  const ConstBufferVector &ToBuffers() const {
    return buffers_;
  }
  bool Encoded() const {
    return encoded_;
  }
 private:
  bool Encode(const google::protobuf::Message *msg) {
    const string &msg_name = msg->GetDescriptor()->full_name();
    return Encode(msg_name, msg);
  }

  bool Encode(const string &name, const google::protobuf::Message *msg) {
    VLOG(3) << "Encode : " << name;
    if (!msg->AppendToString(&body_)) {
      VLOG(3) << "Append Message : " << name << " failed";
      return false;
    }
    if (body_.empty()) {
      VLOG(3) << "message is null";
      return false;
    }
    header_.append(boost::lexical_cast<string>(name.size()));
    header_.append(":");
    header_.append(name);
    header_.append(boost::lexical_cast<string>(body_.size()));
    header_.append(":");
    buffers_.push_back(boost::asio::const_buffer(header_.c_str(), header_.size()));
    buffers_.push_back(boost::asio::const_buffer(body_.c_str(), body_.size()));
    VLOG(3) << "header: " << header_;
    VLOG(3) << "body: " << body_ << boost::asio::buffer_size(buffers_[1]);
    return true;
  }
  const google::protobuf::Message *msg_;
  shared_ptr<google::protobuf::Message> shared_msg_;
  string header_, body_;
  ConstBufferVector buffers_;
  bool encoded_;
};

struct ProtobufLineFormat;
class ProtobufLineFormatParser {
 public:
  /// Construct ready to parse the request method.
  ProtobufLineFormatParser();

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

struct ProtobufLineFormat {
  typedef ProtobufEncoder Encoder;
  typedef ProtobufLineFormatParser Parser;
  string name_length_store;
  int name_length;
  Buffer<char> name_store;
  string name;
  string body_length_store;
  int body_length;
  Buffer<char> body_store;
  string body;
};

class ProtobufConnection;
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

  bool IsRunning() const {
    return status_ == RUNNING;
  }

  void PushMessage(const string &name, const google::protobuf::Message *msg) {
    boost::mutex::scoped_lock locker(encoder_list_mutex_);
    encoders_.push_back(shared_ptr<ProtobufEncoder>(new ProtobufEncoder(name, msg)));
    VLOG(2) << "encoder size: " << encoders_.size();
  }

  void PushMessage(const google::protobuf::Message *msg) {
    return PushMessage(msg->GetDescriptor()->full_name(), msg);
  }

  void PushMessage(const string &name, shared_ptr<google::protobuf::Message> msg) {
    boost::mutex::scoped_lock locker(encoder_list_mutex_);
    encoders_.push_back(shared_ptr<ProtobufEncoder>(new ProtobufEncoder(name, msg)));
  }

  void PushMessage(shared_ptr<google::protobuf::Message> msg) {
    return PushMessage(msg->GetDescriptor()->full_name(), msg);
  }

  shared_ptr<ProtobufEncoder> PopEncoder() {
    boost::mutex::scoped_lock locker(encoder_list_mutex_);
    VLOG(2) << "PopEncoder size:" << encoders_.size();
    if (encoders_.empty()) {
      VLOG(2) << "encoders empty";
      return shared_ptr<ProtobufEncoder>(static_cast<ProtobufEncoder*>(NULL));
    }
    shared_ptr<ProtobufEncoder> ret = encoders_.front();
    VLOG(2) << "encoders size:" << encoders_.size();
    encoders_.pop_front();
    VLOG(2) << "encoders size:" << encoders_.size();
    return ret;
  }
 private:
  boost::mutex encoder_list_mutex_;
  std::list<shared_ptr<ProtobufEncoder> > encoders_;
  Status status_;
  ReplyStatus reply_status_;
  friend class ProtobufConnection;
};

typedef boost::function3<
void, const ProtobufLineFormat&, FullDualChannel *,
  ProtobufReply*> ProtobufLineFormatHandler;
class ProtobufResponseQueue : public boost::enable_shared_from_this<ProtobufResponseQueue> {
 public:
  typedef boost::function1<void, const ProtobufLineFormat&> ResponseHandler;
  void HandleResponse(const ProtobufLineFormat &line_format,
                      FullDualChannel *connection,
                      ProtobufReply *reply) {
    VLOG(2) << "Handler response";
    boost::mutex::scoped_lock locker_(response_queue_mutex_);
    if (response_queue_.empty()) {
      LOG(WARNING) << "response queue is empty";
      return;
    }
    ResponseHandler handler = response_queue_.front();
    response_queue_.pop_front();
    handler(line_format);
  }

  void PushResponseCallback(const ResponseHandler &handler) {
    boost::mutex::scoped_lock locker_(response_queue_mutex_);
    response_queue_.push_back(handler);
  }
 private:
  list<ResponseHandler> response_queue_;
  boost::mutex response_queue_mutex_;
};

class ProtobufHandler {
 private:
  typedef hash_map<string, ProtobufLineFormatHandler> HandlerTable;
  typedef hash_map<string, shared_ptr<ProtobufResponseQueue> >
    ResponseHandlerTable;
 public:
  ProtobufHandler()
    : handler_table_(new HandlerTable),
      response_handler_table_(new ResponseHandlerTable) {
  }
  bool HandleService(google::protobuf::Service *service,
                     const google::protobuf::MethodDescriptor *method,
                     const google::protobuf::Message *request_prototype,
                     const google::protobuf::Message *response_prototype,
                     const ProtobufLineFormat &protobuf_request,
                     FullDualChannel *connection,
                     ProtobufReply *reply);

  void HandleLineFormat(const ProtobufLineFormat &request, FullDualChannel *channel, ProtobufReply *reply);
  void RegisterService(google::protobuf::Service *service);
  bool PushResponseCallback(
      const string &name,
      const ProtobufResponseQueue::ResponseHandler &call);
 private:
  void CallServiceMethodDone(google::protobuf::Message *request,
                             google::protobuf::Message *response,
                             ProtobufReply *reply);
  shared_ptr<HandlerTable> handler_table_;
  shared_ptr<ResponseHandlerTable> response_handler_table_;
};

class ProtobufConnection : public ConnectionImpl<
  ProtobufLineFormat, ProtobufHandler, ProtobufReply> {
 public:
  Connection *Clone() {
    ProtobufConnection *connection = new ProtobufConnection;
    connection->handler_ = handler_;
    return connection;
  }
  void RegisterService(google::protobuf::Service *service) {
    handler_.RegisterService(service);
  }
  void CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done);

  void CallMethodCallback(
      const ProtobufLineFormat &lineformat,
      google::protobuf::RpcController *controller,
      google::protobuf::Message *response,
      google::protobuf::Closure *done);
};
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
