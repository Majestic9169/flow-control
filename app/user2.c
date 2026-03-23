/**
 * @file user2.c
 * @brief Receives a file from user1 over a KTP socket
 * @note usage: ./user2 <src_port> <dst_ip> <dst_port> <outfile>
 */

#include "../lib/ksocket.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LOG(...) (printf("[INFO]: "), printf(__VA_ARGS__))
#define RECV_LOG(...) (printf("[RECV]: "), printf(__VA_ARGS__))
#define SLEEP_TIME 1000
#define MAX_WAIT 30

volatile sig_atomic_t is_exit = 0;

void sigint_handler(int sig) {
  (void)sig;
  is_exit = 1;
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    fprintf(stderr, "usage: %s <src_port> <dst_ip> <dst_port> <outfile>\n",
            argv[0]);
    return 1;
  }

  const char *src_port = argv[1];
  const char *dst_ip = argv[2];
  const char *dst_port = argv[3];
  const char *filepath = argv[4];

  FILE *f = fopen(filepath, "wb");
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
  LOG("bound to source %s:%d\n", srcstr, ntohs(ipv4->sin_port));
  ipv4 = (struct sockaddr_in *)dst->ai_addr;
  inet_ntop(dst->ai_family, &(ipv4->sin_addr), dststr, sizeof(dststr));
  LOG("receiving from %s:%d and storing at %s\n", dststr,
      ntohs(((struct sockaddr_in *)dst->ai_addr)->sin_port), filepath);

  /* receiver loop */
  struct sigaction act;
  act.sa_handler = sigint_handler;
  act.sa_flags = SA_RESTART;
  sigemptyset(&act.sa_mask);
  if (sigaction(SIGINT, &act, NULL) == -1) {
    perror("sigaction");
    return 7;
  }

  char buf[MSG_SIZE];
  int msgs = 0;
  int wait_time = 0;
  while (!is_exit) {
    if (k_recvfrom(s, buf, MSG_SIZE, 0, NULL, 0) == -1) {
      if (errno == ENOMESSAGE) {
        if (wait_time > MAX_WAIT) {
          break;
        }
        RECV_LOG("[ENOMESSAGE]: sleeping for %d ms\n", SLEEP_TIME);
        usleep(SLEEP_TIME * 1000);
        wait_time++;
        continue;
      }
      RECV_LOG("[ERROR] k_recvfrom: %s\n", k_strerr(errno));
      break;
    }
    fwrite(buf, 1, MSG_SIZE, f);
    fflush(f);
    msgs++;
    RECV_LOG("Received segment id %d\n", msgs);
    wait_time = 0;
  }

  LOG("Transmission ended | received %d segments | %d bytes\n", msgs,
      msgs * MSG_SIZE);
  fclose(f);
  k_close(s);
  return 0;
}
