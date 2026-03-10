/**
 * @file kinternal.h
 * @brief internal definitions that should not be exposed to the user
 */

#ifndef K_INTERNAL_H
#define K_INTERNAL_H

#include "ksocket.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#define WIN_SIZE 10

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
typedef struct {
  int is_free;          ///< availability flag
  pid_t parent_process; ///< pid of process that created the socket
  int udp_sockfd;       ///< socket file descriptor of underlying udp socket
  struct sockaddr_storage dst_addr; ///< destination address information
  __buf_t send_buf;                 ///< internal transmission buffer
  __buf_t recv_buf;                 ///< internal reception buffer
  __swnd_t swnd;                    ///< sending window
  __rwnd_t rwnd;                    ///< receiving window
  uint8_t nospace; ///< indicate whether space is available in the recv
                   ///< buf or not
} __k_socket_t;

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
void init_buf(__buf_t *buf) {
  memset(buf->buffer, 0, sizeof(buf->buffer));
  buf->left = 0;
  buf->right = 0;
  buf->count = 0;
}

/** @brief push to circular buffer
 *
 * @param buf buffer to push to
 * @param num sequence number to push to window
 *
 * @return -1 and set errno on ENOSPACE, 0 on success
 */
int push_buf(__buf_t *buf, char *msg) {
  if (buf->count >= WIN_SIZE) {
    errno = ENOSPACE;
    return -1;
  }

  memcpy(&buf->buffer[buf->right * MSG_SIZE], msg, MSG_SIZE);
  buf->right = (buf->right + 1) % WIN_SIZE;
  buf->count++;

  return 0;
}

/** @brief pop from circular buffer
 * @param buf buffer to pop from
 * @param dst buffer to copy msg into
 * @return -1 on error, 0 on success
 */
int pop_buf(__buf_t *buf, char *dst) {
  if (buf->count <= 0) {
    errno = ENOMESSAGE;
    return NULL;
  }

  memcpy(dst, &buf->buffer[buf->left * MSG_SIZE], MSG_SIZE);

  buf->left = (buf->left + 1) % WIN_SIZE;
  buf->count--;

  return 0;
}

#endif
