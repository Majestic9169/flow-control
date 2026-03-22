/**
 * @file ksocket.h
 * @brief The interface for the libksocket.a library
 */

#ifndef KSOCKET_H
#define KSOCKET_H

#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define SOCK_KTP 42
#define MSG_SIZE 512
#define T 5
#define DROP_PROBAB 0.05f

/**
 * @brief custom error codes
 * @anchor custom_err
 * @details These start at 200 to avoid system error overlaps (those go upto
 * ~133)
 * @name custom KTP errors
 * @{
 */
#define ENOSPACE 200   ///< No free space available in the required buffer
#define ENOTBOUND 201  ///< The queried address is not bound to the socket
#define ENOMESSAGE 202 ///< No message is available in the message buffer
/**
 * @}
 */

/** @brief return error messages for custom error codes
 *
 * @details get the string description for the custom errors defined for KTP
 * @ref custom_err
 *
 * @param err the global error number variable to check (errno)
 */
const char *k_strerr(int err);

/**@brief create a KTP socket
 *
 * @details open a udp socket with SOCK_KTP type and create an entry for it in
 * the system shared memory
 *
 * @param domain The protocol "family" for communication. Should be AF_INET or
 * AF_INET6
 * @param type The type of socket, should be SOCKET_KTP
 * @param protocol The protocol to be used with the socket, SOCKET_KTP has an
 * established protocol and thus this should be 0
 *
 * @return -1 on error (errno = ENOSPACE or others)
 */
int k_socket(int domain, int type, int protocol);

/** @brief bind the socket to a client and server address
 *
 * @details Binds the UDP socket with the source IP and source port, and updates
 * the corresponding system shared memory with the destination IP and
 * destination port
 * The sockaddr's will be pointers to sockaddr_in's and therefore should contain
 * port data as well
 *
 * @param sockfd The KTP socket
 * @param src_addr sockaddr to source IP address
 * @param src_addr_len socklen_t of source address
 * @param dst_addr sockaddr to destination IP address
 * @param dst_addr_len socklen_t of destination IP address
 *
 * @return -1 on error
 */
int k_bind(int sockfd, const struct sockaddr *src_addr,
           const socklen_t src_addr_len, const struct sockaddr *dst_addr,
           const socklen_t dst_addr_len);

/** @brief send msg to server buffer
 *
 * @details on match of destination address to bound destination address.
 * Encapsulate the message with the KTP header. The msg is written to the
 * server's recv buffer. Drops message on failure
 *
 * @param socket KTP socket
 * @param message pointer to your 512 byte message
 * @param length message length (512 bytes)
 * @param flags usually 0, could be MSG_EOR, MSG_OOB, MSG_NOSIGNAL
 * @param dest_addr destination address
 * @param dest_len destination address length
 *
 * @return -1 on failure due to addr mismatch (ENOTBOUND) or insufficient space
 * (ENOSPACE)
 */
ssize_t k_sendto(int socket, const void *message, size_t length, int flags,
                 const struct sockaddr *dest_addr, socklen_t dest_len);

/** @brief read message from received messages
 *
 * @details pop top message from receiver buffer
 *
 * @param socket KTP socket
 * @param buffer pointer to your recv buffer
 * @param length message length (512 bytes)
 * @param flags usually 0, could be MSG_PEEK, MSG_OOB, MSG_WAITALL
 * @param address source address
 * @param address_len source address length
 *
 * @return -1 on error, due to no message in buffer is ENOMESSAGE
 */
ssize_t k_recvfrom(int socket, void *buffer, size_t length, int flags,
                   const struct sockaddr *address, socklen_t address_len);

/** @brief close KTP socket
 *
 * @details close KTP socket and clean up its system shared memory entry
 *
 * @param fd file descriptor of KTP socket
 */
int k_close(int fd);

/** @brief drop a msg with a certain probab p
 *
 * @details To simulate unreliable link, this function generates 
 * a random num between 0 and 1 and if that num is less than p, 
 * then drop the msg.
 *
 * @param p probability of dropping (float)
 * 
 * @return 1 if the msg to be dropped, else 0
 */
int dropMessage(float p);

#endif
