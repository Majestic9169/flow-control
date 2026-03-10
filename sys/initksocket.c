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
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
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

socket_table *init_socktable(int shmid, size_t size) {
  socket_table *ptr;

  if (ftruncate(shmid, size) == -1) {
    return NULL;
  }

  ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
  if (ptr == MAP_FAILED) {
    return NULL;
  }

  return ptr;
}

int destroy_socktable(const char *name, socket_table *socktable, size_t size) {
  if (munmap(socktable, size) == -1) {
    return -1;
  }

  if (shm_unlink(name) == -1) {
    return -1;
  }

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

int main(void) {
  signal(SIGINT, sigint_handler);

  int shmid;

  if ((shmid = create_socktable(SOCKTABLE_NAME, 0644)) == -1) {
    ERROR("create_socktable: %s", k_strerr(errno));
    goto exit_0;
  }

  INFO("shm: created shm segment %s", SOCKTABLE_NAME);

  socket_table *socktable;
  if ((socktable = init_socktable(shmid, sizeof(socket_table))) == NULL) {
    ERROR("init_socktable: %s", k_strerr(errno));
    goto exit_1;
  }

  INFO("shm: initialised socket table");

  pthread_t R, S, G;
  pthread_create(&R, NULL, recv_routine, NULL);
  pthread_create(&S, NULL, send_routine, NULL);
  pthread_create(&G, NULL, garbage_collector, NULL);

  while (!is_exit) {
    pause();
  }

  pthread_join(R, NULL);
  pthread_join(S, NULL);
  pthread_join(G, NULL);

  if (destroy_socktable(SOCKTABLE_NAME, socktable, sizeof(socket_table)) ==
      -1) {
    ERROR("destroy_socktable: %s", k_strerr(errno));
  }
exit_1:
  shm_unlink(SOCKTABLE_NAME);
  INFO("shm: destroyed socket table shared memory");
exit_0:
  return 0;
}
