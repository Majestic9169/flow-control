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
#include <stdlib.h>
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
      t->sockets[i].next_seq = 1;
      t->sockets[i].swnd.size = WIN_SIZE;
      t->sockets[i].rwnd.size = WIN_SIZE;
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

/**
 * @details writes message to the sender-side snd buffer.
 *
 * @note doesn't send over UDP directly. Thread S should read
 * from send_buf, adds the KTP header, and call sendto() itself.
 */
ssize_t k_sendto(int socket, const void *message, size_t length, int flags,
                 const struct sockaddr *dest_addr, socklen_t dest_len) {

  /* to supress the unused params warning */
  (void)flags;
  (void)dest_len;

  struct sockaddr_in *ipv4 = (struct sockaddr_in *)dest_addr;

  /* grab table */
  socket_table_t *t = attach_table(SOCKTABLE_NAME, 0644);
  if (t == NULL) {
    return -1;
  }

  /* check destination matches bound address */
  if (t->sockets[socket].dst_addr.sin_addr.s_addr != ipv4->sin_addr.s_addr ||
      t->sockets[socket].dst_addr.sin_port != ipv4->sin_port) {
    errno = ENOTBOUND;
    munmap(t, sizeof(socket_table_t));
    return -1;
  }

  /* grab table mutex before touching snd buffer */
  sem_wait(&t->mtx);

  /* write message to snd buffer and ENOSPACE thing is set by push_buf */
  if (push_buf(&t->sockets[socket].send_buf, message) == -1) {
    sem_post(&t->mtx);
    munmap(t, sizeof(socket_table_t));
    return -1;
  }

  sem_post(&t->mtx);
  munmap(t, sizeof(socket_table_t));

  return (ssize_t)length;
}

/**
 * @details pops the oldest message from the recv buffer.
 * Returns immediately n does not block if no msg available.
 *
 * @return the return value is -1 on errors, or MSG_SIZE on success since the
 * popped packet must always be of size MSG_SIZE
 *
 * @note the value of length **IS NOT** set to MSG_SIZE and is essentially
 * ignored by the function
 */
ssize_t k_recvfrom(int socket, void *buffer, size_t length, int flags,
                   const struct sockaddr *address, socklen_t address_len) {

  /* to supress the unused params warning */
  (void)flags;
  (void)address;
  (void)address_len;
  (void)length;

  /* grab table */
  socket_table_t *t = attach_table(SOCKTABLE_NAME, 0644);
  if (t == NULL) {
    return -1;
  }

  /* grab mutex before touching recv buffer */
  sem_wait(&t->mtx);

  /* pop oldest message and pop_buf sets ENOMESSAGE if no msg in our msg buffr
   */
  if (pop_buf(&t->sockets[socket].recv_buf, buffer) == -1) {
    int savederrno = errno;
    sem_post(&t->mtx);
    munmap(t, sizeof(socket_table_t));
    errno = savederrno;
    return -1;
  }

  sem_post(&t->mtx);
  munmap(t, sizeof(socket_table_t));

  return 512;
}

/**
 * @details signals the daemon to close the underlying UDP socket and
 * free the shared mem for the KTP socket.
 */
int k_close(int fd) {

  /* grab table n error checks */
  socket_table_t *t = attach_table(SOCKTABLE_NAME, 0644);
  if (t == NULL) {
    return -1;
  }

  if (fd < 0 || fd >= MAX_SOCKETS) {
    errno = ENOTSOCK;
    munmap(t, sizeof(socket_table_t));
    return -1;
  }

  /*
   * Drain phase: to not close sock till S has sent everything and has received
   * ACKs for all messages in transit
   */
  while (1) {
    sem_wait(&t->mtx);
    int send_empty = (t->sockets[fd].send_buf.count == 0);
    int unacked_empty = (t->sockets[fd].swnd.unacked_count == 0);
    sem_post(&t->mtx);

    if (send_empty && unacked_empty)
      break;

    /* check every T/4 secs (T/4 should be fine ig, lesser would waste time as
     * blocks mtx) */
    usleep((T * 1000000) / 4);
  }

  /* grab mutex */
  sem_wait(&t->mtx);

  /* to signal daemon to close the UDP fd and free the slot */
  t->info[fd] = CLOSE_REQ;
  sem_post(&t->mtx);

  sem_post(&t->sys_sem);
  sem_wait(&t->lib_sem);

  munmap(t, sizeof(socket_table_t));

  return 0;
}

/**
 * @details drop message with a probab p to simulate unreliable link
 */
int dropMessage(float p) { return ((float)rand() / RAND_MAX) < p ? 1 : 0; }
