#ifndef NET2_PROTOBUF_CONNECTION_HPP_
#define NET2_PROTOBUF_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/connection.hpp"
#include <server/meta.pb.h>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <glog/logging.h>
#include <protobuf/message.h>
#include <protobuf/descriptor.h>
#include <protobuf/service.h>
typedef vector<boost::asio::const_buffer> ConstBufferVector;
class ProtobufDecoder : public Object {
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

class ProtobufConnection : public ConnectionImpl<ProtobufDecoder> {
 private:
   typedef hash_map<uint64, boost::function2<void, shared_ptr<const ProtobufDecoder>,
          ProtobufConnection*> > HandlerTable;
 public:
  ProtobufConnection() : ConnectionImpl<ProtobufDecoder>(),
      handler_table_(new HandlerTable) {
  }

  Connection *Clone() {
    ProtobufConnection *connection = new ProtobufConnection;
    connection->handler_table_ = handler_table_;
    return connection;
  }
  // Non thread safe.
  bool RegisterService(google::protobuf::Service *service);
  // Thread safe.
  void CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done);
 private:
  virtual void Handle(shared_ptr<const ProtobufDecoder> decoder);
  void CallMethodCallback(
      shared_ptr<const ProtobufDecoder> decoder,
      ProtobufConnection *,
      google::protobuf::RpcController *controller,
      google::protobuf::Message *response,
      google::protobuf::Closure *done);
  shared_ptr<HandlerTable> handler_table_;
  // The response handler table is per connection.
  HandlerTable response_handler_table_;
  boost::mutex response_handler_table_mutex_;
};
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
