/**
 * @file ksocket.c
 * @brief The source code for the libksocket.a library
 */

#include "ksocket.h"
#include "kinternal.h"
#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @details request a socket from the "init" process and populate its entries
 *
 * @warning we can not directly call "socket" from this function as the file
 * desriptor returned will be unique for this process and unuseable by the init
 * process.
 *
 * we will instead request the init process to create an FD for us by signalling
 * a semaphore, and waiting on another semaphore that tells us when it is ready
 */
int k_socket(int domain, int type, int protocol) {
  if (type != SOCK_KTP) {
    errno = ESOCKTNOSUPPORT;
    return -1;
  }

  socket_table_t *t = attach_table(SOCKTABLE_NAME, 0644);
  if (t == NULL) {
    return -1;
  }

  sem_wait(&t->mtx);
  if (t->count == MAX_SOCKETS) {
    errno = ENOSPACE;
    sem_post(&t->mtx);
    return -1;
  }

  int i;
  for (i = 0; i < MAX_SOCKETS; i++) {
    if (t->sockets[i].is_free) {
      t->sockets[i].is_free = 0;
      t->sockets[i].type = SOCK_KTP;
      t->sockets[i].udp_sockfd = -1;
      t->sockets[i].parent_process = getpid();
      t->sockets[i].domain = domain;
      t->sockets[i].protocol = protocol;
      t->count++;
      sem_post(&t->mtx);
      break;
    }
  }

  sem_post(&t->sys_sem);
  sem_wait(&t->lib_sem);

  munmap(t, sizeof(socket_table_t));

  return i;
}

int k_bind(int sockfd, const struct sockaddr *src_addr,
           const socklen_t src_addr_len, const struct sockaddr *dst_addr,
           const socklen_t dst_addr_len) {

  socket_table_t *t = attach_table(SOCKTABLE_NAME, 0644);
  if (t == NULL) {
    return -1;
  }

  if (bind(t->sockets[sockfd].udp_sockfd, src_addr, src_addr_len) == -1) {
    return -1;
  }

  memcpy(&(t->sockets[sockfd].dst_addr), dst_addr, sizeof(struct sockaddr_in));

  munmap(t, sizeof(socket_table_t));

  return 0;
}

ssize_t k_sendto(int socket, const void *message, size_t length, int flags,
                 const struct sockaddr *dest_addr, socklen_t dest_len) {

  ssize_t n;
  struct sockaddr_in *ipv4 = (struct sockaddr_in *)dest_addr;
  socket_table_t *t = attach_table(SOCKTABLE_NAME, 0644);
  if (t == NULL) {
    return -1;
  }

  if (t->sockets[socket].dst_addr.sin_addr.s_addr != ipv4->sin_addr.s_addr ||
      t->sockets[socket].dst_addr.sin_port != ipv4->sin_port) {
    errno = ENOTBOUND;
    return -1;
  }

  if (t->sockets[socket].nospace) {
    errno = ENOSPACE;
    return -1;
  }

  push_buf(&t->sockets[socket].send_buf, message);

  __ktp_header hdr;
  // TODO: encapsulate message with header
  // TODO: window operations
  // TODO: timestamp

  munmap(t, sizeof(socket_table_t));

  return 0;
}
