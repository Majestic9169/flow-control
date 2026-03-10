/**
 * @file initksocket.h
 * @brief simulates the "kernel" and services ktp operation calls
 *
 * @details initializes R and S threads to handle receive and send calls.
 *
 *Initializes shared memory for details regarding SOCK_KTP sockets.
 *
 * Creates a garbage collector thread to clean up SOCK_KTP sockets when their
 * parent process dies
 */

#ifndef INITKSOCKET_H
#define INITKSOCKET_H

#include "../lib/kinternal.h"
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>

#define MAX_SOCKETS 128

/** @brief information logging macros for the init process
 * @anchor custom_log
 * @name Logging Macros
 * @{
 */
#define LOG_TIME()                                                             \
  {                                                                            \
    time_t now = time(NULL);                                                   \
    char buf[20];                                                              \
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&now));                   \
    printf("[%s] ", buf);                                                      \
  }

#define INFO(fmt, ...)                                                         \
  do {                                                                         \
    LOG_TIME();                                                                \
    printf("[%lu] " fmt "\n", (unsigned long)pthread_self(), ##__VA_ARGS__);   \
    fflush(stdout);                                                            \
  } while (0)

#define ERROR(fmt, ...)                                                        \
  do {                                                                         \
    LOG_TIME();                                                                \
    printf("[%lu] [ERROR] " fmt "\n\t\t%s:%d\n",                               \
           (unsigned long)pthread_self(), ##__VA_ARGS__, __FILE__, __LINE__);  \
    fflush(stdout);                                                            \
  } while (0)
/**
 * @}
 */

/** @brief the global state table of KTP sockets
 */
typedef struct {
  __k_socket_t sockets[MAX_SOCKETS]; ///< array of ksockets
  sem_t mtx;                         ///< mutex to protect table access
  size_t count;                      ///< current active sockets
} socket_table_t;

/** @brief create the socket table
 *
 * @details exclusively create a shared memory segment
 *
 * @note using POSIX shm
 *
 * @param name name to refer to the segment
 * @param flags the permissions associated with the shm
 *
 * @return -1 on any sort of error with errno describing the error
 */
int create_socktable(const char *name, int flags);

/** @brief attach and init the socktable
 *
 * @param shmid shmid returned by create_socktable
 * @param size size of shm segment
 *
 * @return NULL on any error with errno describing the error
 */
socket_table_t *init_socktable(int shmid, size_t size);

/** @brief cleanup shared memory
 *
 * @param name of shm segment
 * @param socktable pointer to shared memory
 * @param size size of shm segment
 *
 * @return -1 on any error
 */
int destroy_socktable(const char *name, socket_table_t *socktable, size_t size);

/** @brief the receiver thread routine
 *
 * @details waits for message to come in a recvfrom() call from *any*
 * of the UDP sockets (using select()). When received, append the
 * message to the receive buffer of the corresponding socket and
 * sends the corresponding ACK
 *
 * updates windows
 *
 */
void *recv_routine(void *args);

/** @brief the sender thread routine
 *
 * @details sleeps for time T/2 and wakes up periodically.
 *
 * if message timeout period is over, retransmits swnd of the socket
 */
void *send_routine(void *args);

/** @brief garbage collector routine
 *
 * @details empty up unterminated sockets once their parent processes
 * are killed
 *
 */
void *garbage_collector(void *args);

#endif
