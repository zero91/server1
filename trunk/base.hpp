#ifndef BASE_H_
#define BASE_H_
#include "pcre.h"
#include "pcre_stringpiece.h"
using namespace pcrecpp;

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
using namespace boost;

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

#include "buffer.hpp"
#endif // BASE_H_
