#include "server/protobuf_connection.hpp"
ProtobufRequestParser::ProtobufRequestParser()
  : state_(Start) {
}

void ProtobufRequestParser::reset() {
  state_ = Start;
}

boost::tribool ProtobufRequestParser::Consume(
    ProtobufRequest* req, char input) {
  switch (state_) {
    case Start:
      {
        if (!isdigit(input)) {
          return false;
        }
        state_ = NameLength;
        req->name_length_store.push_back(input);
        return boost::indeterminate;
      }
    case NameLength:
      if (input == ':') {
        state_ = Name;
        req->name_length = boost::lexical_cast<int>(req->name_length_store);
        req->name_store.reserve(req->name_length);
        return boost::indeterminate;
      } else if (!isdigit(input)) {
        return false;
      } else {
        req->name_length_store.push_back(input);
        return boost::indeterminate;
      }
    case Name:
      if (req->name_store.full()) {
        return false;
      }
      req->name_store.push_back(input);
      if (req->name_store.full()) {
        state_ = BodyLength;
      }
      return boost::indeterminate;
    case BodyLength:
      if (input == ':') {
        state_ = Body;
        req->body_length = boost::lexical_cast<int>(req->body_length_store);
        req->body_store.reserve(req->body_length);
        return boost::indeterminate;
      } else if (!isdigit(input)) {
        return false;
      } else {
        req->body_length_store.push_back(input);
        return boost::indeterminate;
      }
    case Body:
      if (req->body_store.full()) {
        return false;
      }
      req->body_store.push_back(input);
      if (req->body_store.full()) {
        state_ = End;
        return true;
      }
      return boost::indeterminate;
    default:
      return false;
  }
}
