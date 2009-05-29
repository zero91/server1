// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef NET2_CONNECTION_HPP_
#define NET2_CONNECTION_HPP_
#include "base/base.hpp"
#include "base/executor.hpp"
#include "glog/logging.h"
#include <boost/shared_ptr.hpp>
class Connection2;
class Connection {
 public:
  Connection(Connection2 *connection);
  void Close(const boost::function0<void> h = boost::function0<void>());
  void set_socket(boost::asio::ip::tcp::socket *socket);
  void set_executor(Executor *executor);
  Executor *executor() const;
  void set_name(const string name);
  const string name() const;
  void push_close_handler(const boost::function0<void> &h);
  void set_flush_handler(const boost::function0<void> &f);
  bool IsConnected();
  Connection* Clone();
  void ScheduleRead();
  void ScheduleWrite();
  void ScheduleFlush();
  virtual  ~Connection();
 protected:
  boost::shared_ptr<Connection2> impl_;
};
#endif // NET2_CONNECTION_HPP_
