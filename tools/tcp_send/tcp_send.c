// tcp_send.c
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: %s host port message\n", argv[0]); return 2; }

  const char *host = argv[1], *port = argv[2], *msg = argv[3];
  struct addrinfo hints = {0}, *res = NULL;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int rc = getaddrinfo(host, port, &hints, &res);
  if (rc) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc)); return 1; }

  int fd = -1;
  for (struct addrinfo *p = res; p; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) continue;
    if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
    close(fd); fd = -1;
  }
  freeaddrinfo(res);
  if (fd < 0) { perror("connect"); return 1; }

  size_t n = strlen(msg);
  if (write(fd, msg, n) != (ssize_t)n) { perror("write"); close(fd); return 1; }
  close(fd);
  return 0;
}
