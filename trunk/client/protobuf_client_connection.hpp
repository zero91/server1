#ifndef PROTOBUF_CLIENT_CONNECTION_HPP_
#define PROTOBUF_CLIENT_CONNECTION_HPP_
#include "server/protobuf_connection.hpp"
#include "client/client_connection.hpp"
typedef ClientConnection<ProtobufLineFormat, ProtobufLineFormatParser>
  ProtobufClientConnection;
#endif  // PROTOBUF_CLIENT_CONNECTION_HPP_
