/**
 * @file kinternal.h
 * @brief internal definitions that should not be exposed to the user
 */

#ifndef K_INTERNAL_H
#define K_INTERNAL_H

#include "ksocket.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define WIN_SIZE 10
#define MAX_SOCKETS 128

#define SOCKTABLE_NAME "/ktp_socket_table"

typedef enum { UNUSED, SOCK_REQ, BIND_REQ, CLOSE_REQ, READY } __k_sock_info;

/** @brief simple circular buffer for sending and receiving buffers
 */
typedef struct {
  char buffer[WIN_SIZE * MSG_SIZE]; ///< messages stored here
  int left;                         ///< front of buffer
  int right;                        ///< back of buffer
  int count;                        ///< count in buffer
} __buf_t;

/**
 * @brief Internal data structure for the sender window
 */
typedef struct {
  size_t size;               ///< max unacked packets that can be sent
  uint8_t unacked[WIN_SIZE]; ///< sequence numbers of all currently
                             ///< unacknowledged messages (circular buffer)
  struct timeval last_sent;  ///< time stamp of the oldest unacked message
} __swnd_t;

/**
 * @brief Internal data structure for the receiver window
 */
typedef struct {
  size_t size;                ///< max number of packets that can be received
  uint8_t expected[WIN_SIZE]; ///< sequence number of all currently expected
                              ///< packets
} __rwnd_t;

/**
 * @brief Internal data stored for each created socket
 * @details This structure tracks the state, buffers, and addressing for each
 * socket
 */
// TODO: sequence and ack numbers?
typedef struct {
  int is_free;          ///< availability flag
  int udp_sockfd;       ///< socket file descriptor of underlying udp socket
  uint8_t nospace;      ///< indicate whether space is available in the recv
                        ///< buf or not
  pid_t parent_process; ///< pid of process that created the socket
  int domain;           ///< domain of underlying UDP socket
  int type;             ///< type of the socket (should exclusively be SOCK_KTP)
  int protocol;         ///< protocol of the underlying sock type (should be 0)
  struct sockaddr_in dst_addr; ///< destination address information
  struct sockaddr_in src_addr; ///< source address information
  __buf_t send_buf;            ///< internal transmission buffer
  __buf_t recv_buf;            ///< internal reception buffer
  __swnd_t swnd;               ///< sending window
  __rwnd_t rwnd;               ///< receiving window
} __k_socket_t;

/** @brief the global state table of KTP sockets
 */
typedef struct {
  sem_t mtx;     ///< mutex to protect table access
  sem_t sys_sem; ///< init daemon waits on this semaphore
  sem_t lib_sem; ///< library functions wait on this semaphore
  size_t count;  ///< current active sockets
  __k_socket_t sockets[MAX_SOCKETS]; ///< array of ksockets
  __k_sock_info info[MAX_SOCKETS];   ///< array to store info of each socket
} socket_table_t;

/** @brief Structure of the KTP Header
 * @details Defines the exact structure for the KTP Header
 * @code
 *  0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            src_port           |          dst_port             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               |               |           |A|S|               |
   |   seq_num     |    ack_num    |   frsvd   |C|Y|    window     |
   |               |               |           |K|N|               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * @endcode
 *
 * @note we do not include the length field since the header and the messages
 * always have a fixed and known length
 */
typedef struct {
  uint16_t src_port; ///< source port
  uint16_t dst_port; ///< destination port
  uint8_t seq_num;   ///< sequence number
  uint8_t ack_num;   ///< acknowledgement number
  uint8_t frsvd : 6; ///< 6 reserved bits for alignment
  uint8_t f_ack : 1; ///< ACK flag bit
  uint8_t f_syn : 1; ///< SYN flag bit
  uint8_t window;    ///< window size
} __ktp_header;

/** @brief init circular buffer
 * @param buf buffer to init
 */
void init_buf(__buf_t *buf);

/** @brief push to circular buffer
 *
 * @param buf buffer to push to
 * @param num sequence number to push to window
 *
 * @return -1 and set errno on ENOSPACE, 0 on success
 */
int push_buf(__buf_t *buf, const char *msg);

/** @brief pop from circular buffer
 * @param buf buffer to pop from
 * @param dst buffer to copy msg into
 * @return -1 on error, 0 on success
 */
int pop_buf(__buf_t *buf, char *dst);

/** @brief attach to socktable
 *
 * @param name name of the shm file
 * @param mode permissions with which to open the file
 *
 * @return NULL on errors
 */
socket_table_t *attach_table(const char *name, int mode);

#endif
