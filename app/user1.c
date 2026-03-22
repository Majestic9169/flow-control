/**
 * @file user1.c
 * @brief Sends a file to user2 over a KTP socket
 * @note usage: ./user1 <src_port> <dst_ip> <dst_port> <file>
 */

#include "../lib/ksocket.h"
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc != 5) {
    fprintf(stderr, "usage: %s <src_port> <dst_ip> <dst_port> <file>\n", argv[0]);
    return 1;
  }

  const char *src_port = argv[1];
  const char *dst_ip   = argv[2];
  const char *dst_port = argv[3];
  const char *filepath = argv[4];

  FILE *f = fopen(filepath, "rb");
  if (!f) { perror("fopen"); return 1; }

  struct addrinfo hints, *src;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_INET;
  hints.ai_protocol = 0;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags    = AI_PASSIVE;
  getaddrinfo(NULL, src_port, &hints, &src);

  struct sockaddr_in dst;
  memset(&dst, 0, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_port   = htons((uint16_t)atoi(dst_port));
  inet_pton(AF_INET, dst_ip, &dst.sin_addr);

  int s = k_socket(src->ai_family, SOCK_KTP, 0);
  if (s == -1) { fprintf(stderr, "k_socket: %s\n", k_strerr(errno)); return 1; }

  if (k_bind(s, src->ai_addr, src->ai_addrlen,
                (struct sockaddr *)&dst, sizeof(dst)) == -1) {
    fprintf(stderr, "k_bind: %s\n", k_strerr(errno)); return 1;
  }

  freeaddrinfo(src);
  fprintf(stderr, "bound, sending %s\n", filepath);

  char buf[MSG_SIZE];
  int msgs = 0;
  size_t n;

  while ((n = fread(buf, 1, MSG_SIZE, f)) > 0) {
    if (n < MSG_SIZE) memset(buf + n, 0, MSG_SIZE - n);

    while (k_sendto(s, buf, MSG_SIZE, 0,
                    (struct sockaddr *)&dst, sizeof(dst)) == -1) {
      if (errno != ENOSPACE) {
        fprintf(stderr, "k_sendto: %s\n", k_strerr(errno));
        fclose(f);
        k_close(s);
        return 1;
      }
      usleep(10000);
    }
    msgs++;
  }

  fprintf(stderr, "done: %d messages sent\n", msgs);
  fclose(f);
  k_close(s);
  return 0;
}