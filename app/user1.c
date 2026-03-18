/**
 * @file user1.c
 * @brief Sends message to user2
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

  int status = getaddrinfo(NULL, "9000", &hints, &src);
  fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));

  int s = k_socket(src->ai_family, SOCK_KTP, 0);
  fprintf(stderr, "s: %s\n", k_strerr(errno));

  getchar();

  /* for now assuming user2 will be on 127.0.0.1:9001 */
  struct sockaddr_in dst;
  memset(&dst, 0, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_port = htons(9001);
  inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);

  printf("trying bind\n");
  int r = k_bind(s, src->ai_addr, src->ai_addrlen,
                    (struct sockaddr *)&dst, sizeof(dst));

  
  freeaddrinfo(src);
  fprintf(stderr, "ready, type messages\n");
 
  char buf[MSG_SIZE];
  while (fgets(buf, sizeof(buf), stdin)) {
    size_t len = strlen(buf);
    memset(buf + len, 0, MSG_SIZE - len);
 
    while (k_sendto(s, buf, MSG_SIZE, 0,
                    (struct sockaddr *)&dst, sizeof(dst)) == -1) {
      if (errno != ENOSPACE) { 
        fprintf(stderr, "k_sendto says: %s\n", k_strerr(errno));
        break;
      }
      usleep(10000);
    }
  }
  k_close(s);
  return 0;
}