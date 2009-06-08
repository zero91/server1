// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef BASE_H_
#define BASE_H_
#include "protobuf/message.h"
#include "boost/tuple/tuple.hpp"
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/function.hpp>
using boost::scoped_ptr;
using boost::scoped_array;

#include <string>
#include <vector>
#include <ext/hash_map>
#include <ext/hash_set>
#include <stdexcept>
using namespace std;
using namespace __gnu_cxx;

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <climits>

#include "buffer.hpp"
#include "hash.hpp"
#include "closure.hpp"
#include "atomic.hpp"
#endif // BASE_H_
