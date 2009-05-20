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
#include <string.h>
class EHttpd {
 public:
  EHttpd(int port,
         int maxbacklog,
         int maxenvents);
  ~EHttpd();
  void Loop(void);
 private:
  bool SetNonBlocking(int fd);
  bool ListenInit(void);
  bool EpollInit(void);

  int sfd_, efd_;
  int port_, maxbacklog_, maxevents_;

  struct epoll_event *events_;
};
bool EHttpd::SetNonBlocking(int fd) {
  int flags;

  if (0 != ioctl(fd, FIONBIO, &flags)) {
    fprintf(stderr,"ioctl");
    return false;
  }

  if ((flags = fcntl(fd,
                     F_GETFL,
                     0)) < 0) {
    close(fd);
    fprintf(stderr,"fcntl F_GETFL");
    return false;
  }

  if (fcntl(fd,
            F_SETFL,
            flags | O_NONBLOCK) < 0) {
    close(fd);
    fprintf(stderr,"fcntl F_SETFL");
    return false;
  }

  static struct linger ling = {0, 0};


  flags = 1;
  setsockopt(sfd_,
             SOL_SOCKET,
             SO_REUSEADDR,
             &flags,
             sizeof (flags));

  setsockopt(sfd_,
            SOL_SOCKET,
            SO_KEEPALIVE,
            &flags,
            sizeof (flags));

  setsockopt(sfd_,
            SOL_SOCKET,
            SO_LINGER,
            &ling,
            sizeof (ling));

  return true;
}

bool EHttpd::ListenInit(void) {
  sfd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == sfd_) {
    fprintf(stderr,"socket");
    return false;
  }

  if (!SetNonBlocking(sfd_)) {
    return false;
  }
  
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd_,
           (struct sockaddr*)&addr,
           sizeof (addr)) == -1) {
    close(sfd_);
    fprintf(stderr,"bind");
    return false;
  }

  listen(sfd_, maxbacklog_);
  return true;
}

bool EHttpd::EpollInit() {
  efd_ = epoll_create(maxbacklog_);
  if (efd_ < 0) {
    fprintf(stderr,"epoll_create");
    return false;
  }
  return true;
}

EHttpd::EHttpd(int port,
               int maxbacklog,
               int maxevents)
    : port_(port),
      maxbacklog_(maxbacklog),
      maxevents_(maxevents) {
  if (!ListenInit()) {
    exit (-1);
  }

  if (!EpollInit()) {
    exit (-1);
  }

  events_ = new struct epoll_event[maxevents_];
}

EHttpd::~EHttpd() {
  delete []events_;
}

void EHttpd::Loop(void) {
  static const char welcome[] = "hello world";
  struct epoll_event *e;
  struct epoll_event ev;
  int fd;
  ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
  ev.data.ptr = reinterpret_cast<void*>(sfd_);
  if (epoll_ctl(efd_,
                EPOLL_CTL_ADD,
                sfd_,
                &ev) < 0) {
    fprintf(stderr,"error add listen fd");
    exit (-1);
  }

  struct sockaddr_in addr;
  socklen_t addrlen = sizeof (addr);

  int events;
  for(;;) {
    int nfds = epoll_wait(efd_,
                          events_,
                          maxevents_,
                          -1);
    if (nfds == -1) {
      fprintf(stderr,"wakeup");
      continue;
    }
    for (int i = 0; i < nfds; ++i) {
      e = &events_[i];
      events = e->events;
      fd = reinterpret_cast<int>(e->data.ptr);
      if (fd != sfd_) {
        // It is not the listen connection
        // A in/out event
        char buffer[1024] = { 0 };
        int n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
          printf("read:%d [%s]\n", n, buffer);
          write(fd, buffer, n);
        } else {
          printf("n=%d, error:%d:%s\n", n, errno, strerror(errno));
        }
        continue;
      }

      // Accept a connection
      addrlen = sizeof (addr);
      fd = accept(sfd_,
                  (struct sockaddr*) &addr,
                  &addrlen);
      if (fd < 0) {
        fprintf(stderr,"accept");
        continue;
      }
      printf("accept connect from : %s\n",
            inet_ntoa(addr.sin_addr));

      if (!SetNonBlocking(fd)) {
        fprintf(stderr,"set nonblocking");
        continue;
      }
      ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
      ev.data.ptr = reinterpret_cast<void*>(fd);
      if (epoll_ctl(efd_,
                    EPOLL_CTL_ADD,
                    fd,
                    &ev) < 0) {
        fprintf(stderr,
                "epoll set insertion error: fd = %d\n",
                fd);
      }
      write(fd, welcome, sizeof(welcome));
    }
  }
}

static short port = 8888;
// cat /proc/sys/net/ipv4/tcp_max_syn_backlog
static int maxbacklog = 102400;
static int maxevents = maxbacklog;

int main(int argc, char **argv) {
  EHttpd ed(port,
            maxbacklog,
            maxevents);
  ed.Loop();
  return (0);
}
