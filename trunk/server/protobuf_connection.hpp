// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef NET2_PROTOBUF_CONNECTION_HPP_
#define NET2_PROTOBUF_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/connection.hpp"
#include <server/meta.pb.h>
#include <boost/function.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <glog/logging.h>
class ProtobufDecoder;
class ProtobufConnection : public Connection {
 private:
  typedef boost::function2<void,
          const ProtobufDecoder*,
          boost::shared_ptr<Connection> > HandlerFunctor;
  typedef hash_map<uint64, HandlerFunctor> HandlerTable;
 public:
  static const int kTimeoutMs = 3000;
  explicit ProtobufConnection(const string &name)
    : Connection(name) {
    VLOG(2) << "New protobuf connection: " << name;
  }

  ~ProtobufConnection() {
  }
  virtual boost::shared_ptr<Connection> Span(
      boost::shared_ptr<Timer> timer,
      boost::asio::ip::tcp::socket *socket);
  bool Attach(
      boost::shared_ptr<Timer> timer,
      ProtobufConnection *service_connection,
      boost::asio::ip::tcp::socket *socket);

  // Non thread safe.
  bool RegisterService(google::protobuf::Service *service);
  // Thread safe.
  void CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done);
 private:
  virtual bool Handle(boost::shared_ptr<Connection> connection,
                      const ProtobufDecoder *decoder) const;
  scoped_ptr<HandlerTable> handler_table_;
  friend class RawProtobufConnection;
};
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
