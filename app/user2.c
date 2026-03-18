/**
 * @file user2.c
 * @brief Received msgs from user1
 * @note Just a prototype for testing. Hardcoded local host and ports for now.
 */

#include "../lib/ksocket.h"
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
  printf("hello\n");

  struct addrinfo hints, *src, *p;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_protocol = 0;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(NULL, "9001", &hints, &src);
  fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));

  int s = k_socket(src->ai_family, SOCK_KTP, 0);
  fprintf(stderr, "s: %s\n", k_strerr(errno));

  getchar();

  struct sockaddr_in dst;
  memset(&dst, 0, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_port = htons(9000);
  inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);

  printf("trying bind\n");
  int r = k_bind(s, src->ai_addr, src->ai_addrlen,
                    (struct sockaddr *)&dst, sizeof(dst));

  freeaddrinfo(src);
  fprintf(stderr, "waiting for messages\n");

  char buf[MSG_SIZE];
  while (1) {
    if (k_recvfrom(s, buf, MSG_SIZE, 0, NULL, 0) == -1) {
      if (errno == ENOMESSAGE) { usleep(10000); continue; }
      fprintf(stderr, "k_recvfrom: %s\n", k_strerr(errno));
      break;
    }
    printf("received: %s\n", buf);
  }

  k_close(s);
  return 0;
}