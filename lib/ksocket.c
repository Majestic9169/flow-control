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

/** @name Internal Helpers
 * Helpers for accessing table etc
 * @{
 */
void init_buf(__buf_t *buf) { memset(buf, 0, sizeof(__buf_t)); }

int push_buf(__buf_t *buf, const char *msg) {
  if (buf->count >= WIN_SIZE) {
    errno = ENOSPACE;
    return -1;
  }

  memcpy(&buf->buffer[buf->right * MSG_SIZE], msg, MSG_SIZE);
  buf->right = (buf->right + 1) % WIN_SIZE;
  buf->count++;

  return 0;
}

int pop_buf(__buf_t *buf, char *dst) {
  if (buf->count <= 0) {
    errno = ENOMESSAGE;
    return -1;
  }

  memcpy(dst, &buf->buffer[buf->left * MSG_SIZE], MSG_SIZE);

  buf->left = (buf->left + 1) % WIN_SIZE;
  buf->count--;

  return 0;
}

socket_table_t *attach_table(const char *name, int mode) {
  int shmid;
  socket_table_t *t;

  if ((shmid = shm_open(name, O_RDWR, mode)) == -1) {
    return NULL;
  }

  if ((t = mmap(0, sizeof(socket_table_t), PROT_WRITE | PROT_READ, MAP_SHARED,
                shmid, 0)) == MAP_FAILED) {
    return NULL;
  }

  close(shmid);
  return t;
}

const char *k_strerr(int err) {
  switch (err) {
  case ENOSPACE:
    return "No space available in memory";
  case ENOTBOUND:
    return "The requested destination address is not bound to the socket";
  case ENOMESSAGE:
    return "No message is available in the message buffer";
  default:
    return strerror(err);
  }
}
/**
 * @}
 */

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
  /* check for sock type */
  if (type != SOCK_KTP) {
    errno = ESOCKTNOSUPPORT;
    return -1;
  }

  /* grab table */
  socket_table_t *t = attach_table(SOCKTABLE_NAME, 0644);
  if (t == NULL) {
    return -1;
  }

  /* grab mutex */
  sem_wait(&t->mtx);

  /* check sockets are available */
  if (t->count == MAX_SOCKETS) {
    errno = ENOSPACE;
    sem_post(&t->mtx);
    return -1;
  }

  /* find free socket */
  int i;
  for (i = 0; i < MAX_SOCKETS; i++) {
    if (t->sockets[i].is_free) {
      t->info[i] = SOCK_REQ;
      t->sockets[i].type = SOCK_KTP;
      t->sockets[i].parent_process = getpid();
      t->sockets[i].domain = domain;
      t->sockets[i].protocol = protocol;
      t->count++;
      break;
    }
  }

  /* req daemon for socket */
  sem_post(&t->sys_sem);

  /* wait for response */
  sem_wait(&t->lib_sem);

  /* release mutex */
  sem_post(&t->mtx);

  /* detach table */
  munmap(t, sizeof(socket_table_t));

  return i;
}

int k_bind(int sockfd, const struct sockaddr *src_addr,
           const socklen_t src_addr_len, const struct sockaddr *dst_addr,
           const socklen_t dst_addr_len) {

  /* grab table */
  socket_table_t *t = attach_table(SOCKTABLE_NAME, 0644);
  if (t == NULL) {
    return -1;
  }

  /* check for invalid sockfd */
  if (sockfd < 0 || sockfd >= MAX_SOCKETS) {
    errno = ENOTSOCK;
    munmap(t, sizeof(socket_table_t));
    return -1;
  }

  /* grab tble mtx */
  sem_wait(&t->mtx);

  /* create bind request */
  memcpy(&(t->sockets[sockfd].src_addr), src_addr, src_addr_len);
  memcpy(&(t->sockets[sockfd].dst_addr), dst_addr, dst_addr_len);
  t->info[sockfd] = BIND_REQ;

  /* signal ksocket daemon */
  sem_post(&t->sys_sem);
  sem_wait(&t->lib_sem);

  /* confirm socket is bound */
  if (t->info[sockfd] != READY) {
    errno = ENOTBOUND;
    sem_post(&t->mtx);
    munmap(t, sizeof(socket_table_t));
    return -1;
  }

  sem_post(&t->mtx);
  munmap(t, sizeof(socket_table_t));

  return 0;
}

/* ssize_t k_sendto(int socket, const void *message, size_t length, int flags,
 */
/*                  const struct sockaddr *dest_addr, socklen_t dest_len) { */
/**/
/*   ssize_t n; */
/*   struct sockaddr_in *ipv4 = (struct sockaddr_in *)dest_addr; */
/*   socket_table_t *t = attach_table(SOCKTABLE_NAME, 0644); */
/*   if (t == NULL) { */
/*     return -1; */
/*   } */
/**/
/*   if (t->sockets[socket].dst_addr.sin_addr.s_addr != ipv4->sin_addr.s_addr ||
 */
/*       t->sockets[socket].dst_addr.sin_port != ipv4->sin_port) { */
/*     errno = ENOTBOUND; */
/*     return -1; */
/*   } */
/**/
/*   if (t->sockets[socket].nospace) { */
/*     errno = ENOSPACE; */
/*     return -1; */
/*   } */
/**/
/*   push_buf(&t->sockets[socket].send_buf, message); */
/**/
/*   __ktp_header hdr; */
/*   // TODO: encapsulate message with header */
/*   // TODO: window operations */
/*   // TODO: timestamp */
/**/
/*   munmap(t, sizeof(socket_table_t)); */
/**/
/*   return 0; */
/* } */
