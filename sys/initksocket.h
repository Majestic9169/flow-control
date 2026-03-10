/**
 * @file initksocket.h
 * @brief simulates the "kernel" and services ktp operation calls
 *
 * @details initializes R and S threads to handle receive and send calls.
 *
 * Initializes shared memory for details regarding SOCK_KTP sockets.
 *
 * Creates a garbage collector thread to clean up SOCK_KTP sockets when their
 * parent process dies
 */

#ifndef INITKSOCKET_H
#define INITKSOCKET_H

#include "../lib/kinternal.h"
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>

#define MAX_SOCKETS 128

/** @brief array of sock_ktp sockets available to the system
 */
typedef __k_socket_t socket_table[MAX_SOCKETS];

/** @brief create the socket table
 *
 * @details exclusively create a shared memory segment
 *
 * @param key the key created by ftok to identify the segment (should not be
 * IPC_PRIVATE, that will prevent the library functions from attach to it since
 * they are not spawned by the init process)
 *
 * @return -1 on any sort of error with errno describing the error
 */
int create_socktable(key_t key);

/** @brief attach and init the socktable
 *
 * @param shmid shmid returned by create_socktable
 *
 * @return NULL on any error with errno describing the error
 */
socket_table *init_socktable(int shmid);

/** @brief cleanup shared memory
 *
 * @param socktable pointer to shared memory
 * @param shmid id of shm segment
 *
 * @return -1 on any error
 */
int destroy_socktable(socket_table *socktable, int shmid);

/** @brief the receiver thread routine
 *
 * @details waits for message to come in a recvfrom() call from *any* of the UDP
 * sockets (using select()). When received, append the message to the receive
 * buffer of the corresponding socket and sends the corresponding ACK
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
 * @details empty up unterminated sockets once their parent processes are killed
 *
 */
void *garbage_collector(void *args);

#endif
