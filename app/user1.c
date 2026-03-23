/**
 * @file user1.c
 * @brief Sends a file to user2 over a KTP socket
 * @details Takes local port, remote ip and port and a file name in arguments,
 * which will be fragmented into 512 Byte chunks and transferred via KTP sockets
 * to the remote IP:port
 *
 * @note usage: ./sender <src_port> <dst_ip> <dst_port> <file>
 */

#include "../lib/ksocket.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LOG(...) (printf("[INFO]: "), printf(__VA_ARGS__))
#define SEND_LOG(...) (printf("[SENT]: "), printf(__VA_ARGS__))
#define SLEEP_TIME 1250

int main(int argc, char *argv[]) {
  /* parse args */
  if (argc != 5) {
    fprintf(stderr, "usage: %s <src_port> <dst_ip> <dst_port> <file>\n",
            argv[0]);
    return 1;
  }
  const char *src_port = argv[1];
  const char *dst_ip = argv[2];
  const char *dst_port = argv[3];
  const char *filepath = argv[4];
  FILE *f = fopen(filepath, "rb");
  if (!f) {
    perror("fopen");
    return 3;
  }

  /* get address info of local and remote */
  struct addrinfo hints, *src, *dst;
  memset(&hints, 0, sizeof(hints));
  int status;
  // addr of source
  hints.ai_family = AF_INET;
  hints.ai_protocol = 0;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  status = getaddrinfo(NULL, src_port, &hints, &src);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 2;
  }
  if (src == NULL) {
    fprintf(stderr, "getaddrinfo: failed to get listener address for source\n");
    return 4;
  }
  // addr of dst
  hints.ai_family = AF_INET;
  hints.ai_protocol = 0;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = 0;
  status = getaddrinfo(dst_ip, dst_port, &hints, &dst);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 2;
  }
  if (dst == NULL) {
    fprintf(stderr, "getaddrinfo: failed to get address of dst %s:%s\n", dst_ip,
            dst_port);
    return 4;
  }

  /* create k socket */
  int s = k_socket(src->ai_family, SOCK_KTP, src->ai_protocol);
  if (s == -1) {
    fprintf(stderr, "k_socket: %s\n", k_strerr(errno));
    return 5;
  }

  /* bind k socket to source address */
  if (k_bind(s, src->ai_addr, src->ai_addrlen, dst->ai_addr, dst->ai_addrlen) ==
      -1) {
    fprintf(stderr, "k_bind: %s\n", k_strerr(errno));
    return 6;
  }
  char srcstr[INET6_ADDRSTRLEN];
  char dststr[INET6_ADDRSTRLEN];
  struct sockaddr_in *ipv4 = (struct sockaddr_in *)src->ai_addr;
  inet_ntop(src->ai_family, &(ipv4->sin_addr), srcstr, sizeof(srcstr));
  ipv4 = (struct sockaddr_in *)dst->ai_addr;
  inet_ntop(dst->ai_family, &(ipv4->sin_addr), dststr, sizeof(dststr));
  LOG("sending file %s from src %s:%d to dst %s:%d\n", filepath, srcstr,
      ntohs(((struct sockaddr_in *)src->ai_addr)->sin_port), dststr,
      ntohs(((struct sockaddr_in *)dst->ai_addr)->sin_port));

  /* send data */
  char buf[MSG_SIZE];
  int msgs = 0;
  long long bytes = 0;
  size_t n;
  while ((n = fread(buf, 1, MSG_SIZE, f)) > 0) {
    /* handle chunk < 512 */
    if (n < MSG_SIZE)
      memset(buf + n, 0, MSG_SIZE - n);

    while (k_sendto(s, buf, MSG_SIZE, 0, dst->ai_addr, dst->ai_addrlen) == -1) {
      if (errno != ENOSPACE) {
        SEND_LOG("[ERROR] k_sendto: %s\n", k_strerr(errno));
        fclose(f);
        k_close(s);
        return 1;
      } else {
        SEND_LOG("[ENOSPACE] k_sendto: sleeping for %d ms\n", SLEEP_TIME);
      }
      usleep(SLEEP_TIME * 1000);
    }
    msgs++;
    SEND_LOG("sent segment number %d of size %lu\n", msgs, n);
    bytes += n;
  }

  freeaddrinfo(src);
  freeaddrinfo(dst);
  LOG("done sending %s: %d packets | %lld bytes sent\n", filepath, msgs, bytes);
  fclose(f);
  k_close(s);
  return 0;
}
