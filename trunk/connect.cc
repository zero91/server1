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
static short port = 8888;
/int main(int argc, char **argv) {
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
}
