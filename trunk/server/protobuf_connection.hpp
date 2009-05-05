#ifndef NET2_PROTOBUF_CONNECTION_HPP_
#define NET2_PROTOBUF_CONNECTION_HPP_
#include "glog/logging.h"
#include "base/base.hpp"
#include "server/connection.hpp"
struct ProtobufRequest {
  string length_store;
  int length;
  Buffer<char> body_store;
};

class ProtobufRequestParser {
 public:
  /// Construct ready to parse the request method.
  ProtobufRequestParser();

  /// Reset to initial parser state.
  void reset();

  /// Parse some data. The tribool return value is true when a complete request
  /// has been parsed, false if the data is invalid, indeterminate when more
  /// data is required. The InputIterator return value indicates how much of the
  /// input has been consumed.
  template <typename InputIterator>
  tuple<tribool, InputIterator> Parse(ProtobufRequest *req,
      InputIterator begin, InputIterator end) {
    while (begin != end) {
      tribool result = Consume(req, *begin++);
      if (result || !result) {
        return make_tuple(result, begin);
      }
    }
    tribool result = indeterminate;
    return make_tuple(result, begin);
  }
private:
  /// Handle the next character of input.
  tribool Consume(ProtobufRequest *req, char input);

  /// The current state of the parser.
  enum State
  {
    Start,
    Length,
    Body,
    End,
  } state_;
};

class ProtobufReply {
 public:
  vector<asio::const_buffer> ToBuffers() {
    vector<asio::const_buffer> ret;
    return ret;
  }
  enum Status {
    BAD_REQUEST,
  };
  static ProtobufReply StockReply(Status status) {
    return ProtobufReply();
  }
};

class ProtobufRequestHandler : private noncopyable {
 public:
  void HandleRequest(const ProtobufRequest &request, ProtobufReply *reply) {
    StringPiece s(request.body_store.content(), request.body_store.capacity());
    VLOG(2) << "Handle request: " << s;
  }
};

typedef ConnectionImpl<
  ProtobufRequest, ProtobufRequestHandler, ProtobufRequestParser,
  ProtobufReply> ProtobufConnection;
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
