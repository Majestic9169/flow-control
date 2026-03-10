/**
 * @file ksocket.c
 * @brief The source code for the libksocket.a library
 */

#include "ksocket.h"

#include <string.h>

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
