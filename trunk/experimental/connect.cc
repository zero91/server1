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
static const short port = 8888;
static const char *host = "localhost";
int main(int argc, char **argv) {

  int servfd,clifd,length = 0;
  struct sockaddr_in servaddr,cliaddr;
  socklen_t socklen = sizeof(servaddr);
  bzero(&servaddr,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  inet_aton(host,&servaddr.sin_addr);
  servaddr.sin_port = htons(port);

  char buf[256];
  for (int i = 0;; ++i) {
    if ((clifd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
      printf("create socket error!\n");
      exit(1);
    }
    bzero(&cliaddr,sizeof(cliaddr));

    cliaddr.sin_family = AF_INET;
    if (connect(clifd,(struct sockaddr*)&servaddr, socklen) < 0) {
      printf("can''''t connect to %s!\n",host);
      exit(1);
    }
    length = recv(clifd,buf,sizeof(buf), 0);

    if (length < 0) {
      printf("error comes when recieve data from server %s!",host);
      exit(1);
    }
    printf("%d:from server %s :\n\t%s ",i, host,buf);
  }
//  close(clifd);
  return 0;
}
