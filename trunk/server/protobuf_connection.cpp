#include "protobuf_connection.hpp"
ProtobufRequestParser::ProtobufRequestParser()
  : state_(Start) {
}

void ProtobufRequestParser::reset() {
  state_ = Start;
}

tribool ProtobufRequestParser::Consume(
    ProtobufRequest* req, char input) {
  switch (state_) {
    case Start:
      {
        if (!isdigit(input)) {
          return false;
        }
        state_ = Length;
        req->length_store.push_back(input);
        return boost::indeterminate;
      }
    case Length:
      if (input == ':') {
        state_ = Body;
        req->length = lexical_cast<int>(req->length_store);
        req->body_store.reserve(req->length);
        return boost::indeterminate;
      } else if (!isdigit(input)) {
        return false;
      } else {
        req->length_store.push_back(input);
        return boost::indeterminate;
      }
    case Body:
      if (req->body_store.full()) {
        state_ = End;
        return true;
      } else {
        req->body_store.push_back(input);
        return boost::indeterminate;
      }
    default:
      return false;
  }
}
