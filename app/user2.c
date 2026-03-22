/**
 * @file user2.c
 * @brief Receives a file from user1 over a KTP socket
 * @note usage: ./user2 <src_port> <dst_ip> <dst_port> <outfile>
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
    fprintf(stderr, "usage: %s <src_port> <dst_ip> <dst_port> <outfile>\n", argv[0]);
    return 1;
  }

  const char *src_port = argv[1];
  const char *dst_ip   = argv[2];
  const char *dst_port = argv[3];
  const char *filepath = argv[4];

  FILE *f = fopen(filepath, "wb");
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
  fprintf(stderr, "bound, waiting for file\n");

  char buf[MSG_SIZE];
  int msgs = 0;

  while (1) {
    if (k_recvfrom(s, buf, MSG_SIZE, 0, NULL, 0) == -1) {
      if (errno == ENOMESSAGE) { usleep(10000); continue; }
      fprintf(stderr, "k_recvfrom: %s\n", k_strerr(errno));
      break;
    }
    fwrite(buf, 1, MSG_SIZE, f);
    msgs++;
    fprintf(stderr, "recv msg %d\n", msgs);
  }

  fprintf(stderr, "done: %d messages received\n", msgs);
  fclose(f);
  k_close(s);
  return 0;
}