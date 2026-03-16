#include "../lib/ksocket.h"
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

int main(void) {
  printf("hello\n");

  struct addrinfo hints, *res, *p;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_protocol = 0;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(NULL, "9000", &hints, &res);
  fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));

  int s = k_socket(res->ai_family, SOCK_KTP, 0);
  fprintf(stderr, "s: %s\n", k_strerr(errno));

  getchar();

  printf("trying bind\n");
  k_bind(s, res->ai_addr, res->ai_addrlen, res->ai_addr, res->ai_addrlen);
  fprintf(stderr, "bind: %s\n", k_strerr(errno));

  getchar();
}
