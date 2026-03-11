/**
 * @file initksocket.c
 * @brief source code for "kernel simulate"
 */

#include "initksocket.h"
#include "kinternal.h"
#include "ksocket.h"
#include <bits/pthreadtypes.h>
#include <fcntl.h>
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

  if (sem_init(&ptr->mtx, 1, 1) == -1) {
    return NULL;
  }
  INFO("shm: initialised table mutex");

  if (sem_init(&ptr->lib_sem, 1, 1) == -1) {
    return NULL;
  }
  INFO("shm: initialised library semaphore");

  if (sem_init(&ptr->sys_sem, 1, 0) == -1) {
    return NULL;
  }
  INFO("shm: initialised daemon semaphore");

  sem_wait(&ptr->mtx);

  memset(ptr, 0, sizeof(socket_table_t));
  for (size_t i = 0; i < MAX_SOCKETS; i++) {
    ptr->sockets[i].is_free = 1;
    init_buf(&(ptr->sockets[i].recv_buf));
    init_buf(&(ptr->sockets[i].send_buf));
  }
  sem_post(&ptr->mtx);

  return ptr;
}

int destroy_socktable(const char *name, socket_table_t *socktable,
                      size_t size) {
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

void *recv_routine(void *args) {
  (void)args;
  INFO("thread: starting Receiver routine");
  INFO("thread: shut down Receiver routine");
  return NULL;
}
void *send_routine(void *args) {
  (void)args;
  INFO("thread: starting Sender routine");
  INFO("thread: shut down Sender routine");
  return NULL;
}
void *garbage_collector(void *args) {
  (void)args;
  INFO("thread: starting Garbage Collector routine");
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
  pthread_create(&R, NULL, recv_routine, NULL);
  pthread_create(&S, NULL, send_routine, NULL);
  pthread_create(&G, NULL, garbage_collector, NULL);
  /**
   * @}
   */

  while (!is_exit) {
    if (sem_trywait(&socktable->sys_sem) != -1) {
      sem_wait(&socktable->mtx);
      for (int i = 0; i < MAX_SOCKETS; i++) {
        /* check for new socket request */
        if (socktable->sockets[i].is_free &&
            socktable->sockets[i].udp_sockfd == -1) {
          INFO("socket: request for new socket %d by %d", i,
               socktable->sockets[i].parent_process);
          int sockfd = socket(socktable->sockets[i].domain, SOCK_DGRAM,
                              socktable->sockets[i].protocol);

          if (sockfd == -1) {
            ERROR("socket: failed to create new socket");
            perror("socket");
          }

          socktable->sockets[i].udp_sockfd = sockfd;

          INFO("socket: created new socket %d", i);
          break;
        } else { /* check for bind request */
          // TODO: handle bind request
        }
      }
      sem_post(&socktable->mtx);
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
