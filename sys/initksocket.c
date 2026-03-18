/**
 * @file initksocket.c
 * @brief source code for "kernel simulate"
 */

#include "initksocket.h"
#include "kinternal.h"
#include "ksocket.h"
#include <arpa/inet.h>
#include <bits/pthreadtypes.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

volatile sig_atomic_t is_exit = 0;

void sigint_handler(int sig) {
  (void)sig;
  is_exit = 1;
}

int create_socktable(const char *name, int flags) {
  int shmid;

  if ((shmid = shm_open(name, O_CREAT | O_RDWR | O_EXCL, flags)) == -1) {
    return -1;
  }

  return shmid;
}

socket_table_t *init_socktable(int shmid, size_t size) {
  socket_table_t *ptr;

  if (ftruncate(shmid, size) == -1) {
    return NULL;
  }
  INFO("shm: resized shm segment");

  ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
  if (ptr == MAP_FAILED) {
    return NULL;
  }
  INFO("shm: attached to shm segment");
  memset(ptr, 0, sizeof(socket_table_t));

  if (sem_init(&ptr->mtx, 1, 1) == -1) {
    return NULL;
  }
  INFO("shm: initialised table mutex");

  if (sem_init(&ptr->lib_sem, 1, 0) == -1) {
    return NULL;
  }
  INFO("shm: initialised library semaphore");

  if (sem_init(&ptr->sys_sem, 1, 0) == -1) {
    return NULL;
  }
  INFO("shm: initialised daemon semaphore");

  sem_wait(&ptr->mtx);

  for (size_t i = 0; i < MAX_SOCKETS; i++) {
    ptr->sockets[i].is_free = 1;
    ptr->info[i] = UNUSED;
    init_buf(&(ptr->sockets[i].recv_buf));
    init_buf(&(ptr->sockets[i].send_buf));
  }
  sem_post(&ptr->mtx);

  return ptr;
}

int destroy_socktable(const char *name, socket_table_t *socktable,
                      size_t size) {
  for (size_t i = 0; i < MAX_SOCKETS; i++) {
    if (socktable->sockets[i].is_free == 0) {
      close(socktable->sockets[i].udp_sockfd);
    }
  }
  INFO("socket: close all open socket fds");

  sem_destroy(&socktable->mtx);
  INFO("shm: destroy table mutex");
  sem_destroy(&socktable->sys_sem);
  INFO("shm: destroy system semaphore");
  sem_destroy(&socktable->lib_sem);
  INFO("shm: destroy library semaphore");

  if (munmap(socktable, size) == -1) {
    return -1;
  }
  INFO("shm: dettach from memory");

  if (shm_unlink(name) == -1) {
    return -1;
  }
  INFO("shm: destroy shm segment");

  return 0;
}

/** @brief build and send a ACK on a socket */
static void send_ack(__k_socket_t *sock, uint8_t ack_num) {
  __ktp_header ack;
  memset(&ack, 0, sizeof(ack));
  ack.src_port = sock->src_addr.sin_port;
  ack.dst_port = sock->dst_addr.sin_port;
  ack.ack_num  = ack_num;
  ack.f_ack    = 1;
  ack.window   = (uint8_t)(WIN_SIZE - sock->recv_buf.count);
  sendto(sock->udp_sockfd, &ack, sizeof(ack), 0,
         (struct sockaddr *)&sock->dst_addr, sizeof(sock->dst_addr));
}

/** @brief receiver thread routine
 *
 * @details waits for a message in a recvfrom() call from any of
 * the UDP socks using select(). Data packets are written to recv_buf
 * or held in rwnd.ooo (out-of-order, so no ACK).
 * On select() timeout, sends dup ACK if nospace has since cleared.
 */
void *recv_routine(void *args) {
  INFO("thread: starting Receiver routine");
  
  socket_table_t *socktable = (socket_table_t *)args;
  char packet[sizeof(__ktp_header) + MSG_SIZE];

  while (!is_exit) {
    /* build fd_set frm active socks */
    fd_set readfds;
    FD_ZERO(&readfds);
    int maxfd = -1;

    sem_wait(&socktable->mtx);
    for (int i = 0; i < MAX_SOCKETS; i++) {
      if (socktable->sockets[i].is_free) continue;
      int fd = socktable->sockets[i].udp_sockfd;
      FD_SET(fd, &readfds);
      if (fd > maxfd) maxfd = fd;
    }
    sem_post(&socktable->mtx);

    if (maxfd == -1) { sleep(1); continue; }

    struct timeval tv = {1, 0}; /* 1.000 secs */
    int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
    if (ready == -1) {
      ERROR("select: %s", strerror(errno));
      continue;
    }

    /* timeout: if recv buffer freed up since nospace was set, unblock sender
     * with a duplicate ACK carrying the updated window */
    if (ready == 0) {
      sem_wait(&socktable->mtx);
      for (int i = 0; i < MAX_SOCKETS; i++) {
        if (socktable->sockets[i].is_free) continue;
        __k_socket_t *sock = &socktable->sockets[i];

        if (sock->nospace && sock->recv_buf.count < WIN_SIZE) {
          sock->nospace = 0;
          send_ack(sock, sock->last_acked_seq);
          INFO("socket %d: nospace cleared, sent dup ACK ack=%u", i,
               sock->last_acked_seq);
        }
      }
      sem_post(&socktable->mtx);
      continue;
    }

    sem_wait(&socktable->mtx);
    /* Now the main recvfrom logic */
    for (int i = 0; i < MAX_SOCKETS; i++) {
      if (socktable->sockets[i].is_free) continue;
      __k_socket_t *sock = &socktable->sockets[i];
      if (!FD_ISSET(sock->udp_sockfd, &readfds)) continue;

      struct sockaddr_in sender;
      socklen_t slen = sizeof(sender);
      ssize_t n = recvfrom(sock->udp_sockfd, packet, sizeof(packet), 0,
                           (struct sockaddr *)&sender, &slen);
      if (n < (ssize_t)sizeof(__ktp_header)) continue;

      /* split packet, jus rip it up */
      __ktp_header *hdr = (__ktp_header *)packet;
      char *payload     = packet + sizeof(__ktp_header);

      if (!hdr->f_ack) {
        /* offset from next expected seq tells us position in window */
        uint8_t seq    = hdr->seq_num;
        uint8_t offset = (uint8_t)(seq - (uint8_t)(sock->last_acked_seq + 1));

        if (offset >= WIN_SIZE) {
          /* duplicate or outside window: re-ACK what we have */
          send_ack(sock, sock->last_acked_seq);

        } else if (offset == 0) {
          /* in-order: deliver and flush any consecutive OOO messages */
          push_buf(&sock->recv_buf, payload);
          sock->last_acked_seq = seq;

          int flushed = 1;
          while (flushed) {
            flushed = 0;
            uint8_t want = (uint8_t)(sock->last_acked_seq + 1);
            for (int j = 0; j < (int)sock->rwnd.ooo_count; j++) {
              if (sock->rwnd.ooo_seq[j] != want) continue;
              push_buf(&sock->recv_buf, sock->rwnd.ooo_data[j]);
              sock->last_acked_seq = want;
              /* shift remaining OOO entries left */
              for (int k = j; k < (int)sock->rwnd.ooo_count - 1; k++) {
                sock->rwnd.ooo_seq[k] = sock->rwnd.ooo_seq[k + 1];
                memcpy(sock->rwnd.ooo_data[k], sock->rwnd.ooo_data[k + 1],
                       MSG_SIZE);
              }
              sock->rwnd.ooo_count--;
              flushed = 1;
              break;
            }
          }

          sock->nospace = (sock->recv_buf.count >= WIN_SIZE) ? 1 : 0;
          send_ack(sock, sock->last_acked_seq);
          INFO("socket %d: DATA seq=%u ACK=%u win=%d", i, seq,
               sock->last_acked_seq, WIN_SIZE - sock->recv_buf.count);

        } else {
          /* out-of-order within window: buffer for later, send no ACK */
          int already = 0;
          for (int j = 0; j < (int)sock->rwnd.ooo_count; j++)
            if (sock->rwnd.ooo_seq[j] == seq) { already = 1; break; }

          if (!already && sock->rwnd.ooo_count < WIN_SIZE) {
            int idx = sock->rwnd.ooo_count;
            sock->rwnd.ooo_seq[idx] = seq;
            memcpy(sock->rwnd.ooo_data[idx], payload, MSG_SIZE);
            sock->rwnd.ooo_count++;
            INFO("socket %d: OOO seq=%u buffered", i, seq);
          }
        }

      } else {
        /* ACK: remove all acknowledged entries from front of unacked list */
        uint8_t ack_num = hdr->ack_num;

        while (sock->swnd.unacked_count > 0) {
          uint8_t oldest = sock->swnd.unacked[0];

          /* if oldest entry is ahead of ack num in circular buffer then don't slide */
          if ((uint8_t)(ack_num - oldest) >= 128) break;

          for (int k = 0; k < (int)sock->swnd.unacked_count - 1; k++) {
            sock->swnd.unacked[k] = sock->swnd.unacked[k + 1];
            memcpy(sock->swnd.unacked_data[k], sock->swnd.unacked_data[k + 1],
                   MSG_SIZE);
          }
          sock->swnd.unacked_count--;
        }
        sock->swnd.size = hdr->window;
        if (sock->swnd.unacked_count == 0)
          memset(&sock->swnd.last_sent, 0, sizeof(sock->swnd.last_sent));
        INFO("socket %d: ACK ack=%u new_win=%u", i, ack_num, hdr->window);
      }
    }
    sem_post(&socktable->mtx);
  }

  INFO("thread: shut down Receiver routine");
  return NULL;
}

/** @brief sender thread routine
 *
 * @details sleeps for time T/2 and wakes up periodically.
 * Retransmits swnd if timeout T is exceeded. Sends any pending messages
 * from send_buf that fit within the current swnd and remote rwnd.
 */
void *send_routine(void *args) {
  INFO("thread: starting Sender routine");
 
  socket_table_t *socktable = (socket_table_t *)args;
  struct timespec sleep_time = {T / 2, 0};
 
  while (!is_exit) {
    nanosleep(&sleep_time, NULL);
 
    sem_wait(&socktable->mtx);
    for (int i = 0; i < MAX_SOCKETS; i++) {
      if (socktable->sockets[i].is_free) continue;
      __k_socket_t *sock = &socktable->sockets[i];
 
      /* retransmit swnd if oldest unacked message timed out */
      if (sock->swnd.unacked_count > 0) {
        struct timeval current;
        gettimeofday(&current, NULL);
        double elapsed =
            (double)(current.tv_sec  - sock->swnd.last_sent.tv_sec) +
            (double)(current.tv_usec - sock->swnd.last_sent.tv_usec) * 1e-6;
 
        if (elapsed > (double)T) {
          INFO("socket %d: timeout, retransmitting %u messages", i,
               sock->swnd.unacked_count);

          for (int j = 0; j < (int)sock->swnd.unacked_count; j++) {
            __ktp_header hdr;
            memset(&hdr, 0, sizeof(hdr));
            hdr.src_port = sock->src_addr.sin_port;
            hdr.dst_port = sock->dst_addr.sin_port;
            hdr.seq_num  = sock->swnd.unacked[j];
            hdr.window   = (uint8_t)sock->rwnd.size;
 
            char pkt[sizeof(__ktp_header) + MSG_SIZE];
            memcpy(pkt, &hdr, sizeof(hdr));
            memcpy(pkt + sizeof(hdr), sock->swnd.unacked_data[j], MSG_SIZE);
            sendto(sock->udp_sockfd, pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&sock->dst_addr, sizeof(sock->dst_addr));
          }
          gettimeofday(&sock->swnd.last_sent, NULL);
        }
      }
 
      /* send new messages within swnd limit and while receiver has space */
      while (sock->send_buf.count > 0 &&
             sock->swnd.unacked_count < (int)sock->swnd.size &&
             sock->swnd.size > 0 && !sock->nospace) {
 
        char payload[MSG_SIZE];
        pop_buf(&sock->send_buf, payload);
 
        /* store payload in swnd for later retransmission if req */
        int idx = sock->swnd.unacked_count;
        sock->swnd.unacked[idx] = sock->next_seq;
        memcpy(sock->swnd.unacked_data[idx], payload, MSG_SIZE);
 
        __ktp_header hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.src_port = sock->src_addr.sin_port;
        hdr.dst_port = sock->dst_addr.sin_port;
        hdr.seq_num  = sock->next_seq;
        hdr.window   = (uint8_t)sock->rwnd.size;
 
        char pkt[sizeof(__ktp_header) + MSG_SIZE];
        memcpy(pkt, &hdr, sizeof(hdr));
        memcpy(pkt + sizeof(hdr), payload, MSG_SIZE);
        sendto(sock->udp_sockfd, pkt, sizeof(pkt), 0,
               (struct sockaddr *)&sock->dst_addr, sizeof(sock->dst_addr));
 
        if (sock->swnd.unacked_count == 0)
          gettimeofday(&sock->swnd.last_sent, NULL);
 
        sock->swnd.unacked_count++;
        sock->next_seq++;
        INFO("socket %d: sent seq=%u unacked=%u", i, sock->next_seq - 1,
             sock->swnd.unacked_count);
      }
    }
    sem_post(&socktable->mtx);
  }
 
  INFO("thread: shut down Sender routine");
  return NULL;
}
 
/** @brief garbage collector routine
 *
 * @details empty up unterminated sockets once their parent processes
 * are killed
 * 
 * @note assignemnt didn't say anyth abt how often, so I just set freq as every T
 */
void *garbage_collector(void *args) {
  INFO("thread: starting Garbage Collector routine");
  
  socket_table_t *socktable = (socket_table_t *)args;
  while (!is_exit) {
    /* garbage collector wakes up every T time */
    sleep(T);

    sem_wait(&socktable->mtx);
    for (int i = 0; i < MAX_SOCKETS; i++) {
      if (socktable->sockets[i].is_free) continue;
      pid_t pid = socktable->sockets[i].parent_process;

      /* kill(pid, 0) checks process existence without sending a signal */
      if (kill(pid, 0) == -1 && errno == ESRCH) {
        INFO("socket %d: parent pid %d dead, reclaiming", i, pid);
        close(socktable->sockets[i].udp_sockfd);
        socktable->sockets[i].is_free = 1;
        init_buf(&socktable->sockets[i].send_buf);
        init_buf(&socktable->sockets[i].recv_buf);
        socktable->info[i] = UNUSED;
        socktable->count--;
      }
    }
    sem_post(&socktable->mtx);
  }
 
  INFO("thread: shut down Garbage Collector routine");
  return NULL;
}

/**
 * @brief Entry point for the ksocket daemon
 * * inits shared memory and other IPC primitives
 * * spawns worker threads
 * * checks for new socket requests
 */
int main(void) {

  /** @name Init Phase
   * setting up IPC resources and signal handlers
   * @{
   */
  signal(SIGINT, sigint_handler);

  int shmid;

  if ((shmid = create_socktable(SOCKTABLE_NAME, 0644)) == -1) {
    ERROR("create_socktable: %s", k_strerr(errno));
    goto exit_0;
  }

  INFO("shm: created shm segment %s", SOCKTABLE_NAME);

  socket_table_t *socktable;
  if ((socktable = init_socktable(shmid, sizeof(socket_table_t))) == NULL) {
    ERROR("init_socktable: %s", k_strerr(errno));
    goto exit_1;
  }

  INFO("shm: initialised socket table with %lu sockets currently active",
       socktable->count);

  pthread_t R, S, G;
  pthread_create(&R, NULL, recv_routine,      (void *)socktable);
  pthread_create(&S, NULL, send_routine,      (void *)socktable);
  pthread_create(&G, NULL, garbage_collector, (void *)socktable);
  /**
   * @}
   */

  while (!is_exit) {
    if (sem_trywait(&socktable->sys_sem) != -1) {
      for (int i = 0; i < MAX_SOCKETS; i++) {
        /* check for new socket request */
        if (socktable->sockets[i].is_free && socktable->info[i] == SOCK_REQ) {
          INFO("socket: request for new socket %d by pid %d", i,
               socktable->sockets[i].parent_process);
          int sockfd = socket(socktable->sockets[i].domain, SOCK_DGRAM,
                              socktable->sockets[i].protocol);

          if (sockfd == -1) {
            ERROR("socket: failed to create new socket");
            perror("socket");
            break;
          }

          socktable->sockets[i].udp_sockfd = sockfd;
          socktable->sockets[i].is_free = 0;
          socktable->info[i] = READY;

          INFO("socket: created new socket %d (udp_sock = %d)", i, sockfd);

          break;
        } else { /* check for bind request */
          if (socktable->info[i] == BIND_REQ) {
            char ipstr[INET_ADDRSTRLEN];
            struct sockaddr_in ipv4 = socktable->sockets[i].src_addr;
            inet_ntop(AF_INET, &(ipv4.sin_addr), ipstr, sizeof(ipstr));
            INFO("socket: request to bind socket %d to %s:%d", i, ipstr,
                 ntohs(ipv4.sin_port));

            if (bind(socktable->sockets[i].udp_sockfd, (struct sockaddr *)&ipv4,
                     sizeof(ipv4)) == -1) {
              ERROR("socket: failed to bind socket %d", i);
              perror("bind");
              break;
            }

            socktable->info[i] = READY;

            INFO("socket: bound socket %d", i);
          }
        }
      }
      sem_post(&socktable->lib_sem);
    } else {
      sleep(1);
    }
  }

  /** @name Destroy Phase
   * safely remove IPC resources
   * @{
   */

  pthread_join(R, NULL);
  pthread_join(S, NULL);
  pthread_join(G, NULL);

  if (destroy_socktable(SOCKTABLE_NAME, socktable, sizeof(socket_table_t)) ==
      -1) {
    ERROR("destroy_socktable: %s", k_strerr(errno));
  }
exit_1:
  shm_unlink(SOCKTABLE_NAME);
  INFO("shm: destroyed socket table shared memory");
  /**
   * @}
   */
exit_0:
  return 0;
}
