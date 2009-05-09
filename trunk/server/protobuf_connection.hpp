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
    VLOG(3) << "header: " << header_ << boost::asio::buffer_size(buffers_[0]);;
    VLOG(3) << "body: " << body_ << boost::asio::buffer_size(buffers_[1]);
    return true;
  }
  const google::protobuf::Message *msg_;
  shared_ptr<google::protobuf::Message> shared_msg_;
  string header_, body_;
  ConstBufferVector buffers_;
  bool encoded_;
};

struct ProtobufLineFormat {
  typedef ProtobufEncoder Encoder;
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

  void Reset() {
    status_ = IDLE;
    reply_status_ = SUCCEED_WITHOUT_CONTENT;
    encoders_.clear();
  }

  bool IsRunning() const {
    return status_ == RUNNING;
  }

  void PushMessage(shared_ptr<google::protobuf::Message> msg) {
    boost::mutex::scoped_lock locker(encoder_list_mutex_);
    encoders_.push_back(shared_ptr<ProtobufEncoder>(new ProtobufEncoder(msg)));
  }

  shared_ptr<ProtobufEncoder> PopEncoder() {
    boost::mutex::scoped_lock locker(encoder_list_mutex_);
    if (encoders_.empty()) {
      return shared_ptr<ProtobufEncoder>(static_cast<ProtobufEncoder*>(NULL));
    }
    shared_ptr<ProtobufEncoder> ret = encoders_.front();
    encoders_.pop_front();
    return ret;
  }
 private:
  boost::mutex encoder_list_mutex_;
  std::list<shared_ptr<ProtobufEncoder> > encoders_;
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
  template <typename MessageType>
  void RegisterListener(const boost::function2<void,
                        shared_ptr<MessageType>,
                        ProtobufReply* > &callback) {
    scoped_ptr<MessageType> msg(new MessageType);
    const string &msg_name = msg->GetDescriptor()->full_name();
    if (handler_table_->find(msg_name) != handler_table_->end()) {
      LOG(WARNING) << "Listen on " << msg_name << " is duplicated.";
      return;
    }
    boost::function2<void, const ProtobufLineFormat&, ProtobufReply*> handler =
      boost::bind(&ProtobufRequestHandler::ReceiveMessage<MessageType>,
                  this,
                  _1,
                  callback, _2);
    handler_table_->insert(make_pair(
        msg_name,
        handler));
  }
 private:
  template <typename MessageType>
  void ReceiveMessage(const ProtobufLineFormat& line_format,
                      boost::function2<void, shared_ptr<MessageType>,
                      ProtobufReply * > callback,
                      ProtobufReply *reply) {
    reply->Reset();
    shared_ptr<MessageType> message(new MessageType);
    if (!message->ParseFromArray(
        line_format.body.c_str(),
        line_format.body.size())) {
      LOG(WARNING) << line_format.name << " invalid format";
      return;
    }
    callback(message, reply);
  }
  void CallServiceMethodDone(boost::tuple<google::protobuf::Message *,
                             google::protobuf::Message *,
                             ProtobufReply *> tuple);
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
  template <class MessageType>
  void RegisterListener(const boost::function2<void,
                        shared_ptr<MessageType>,
                        ProtobufReply*> &callback) {
    boost::mutex::scoped_lock locker(mutex_);
    lineformat_handler_.RegisterListener(callback);
  }

 private:
  boost::mutex mutex_;

};
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
