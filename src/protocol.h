#pragma once

#include <stddef.h>

/* Client-to-server stream: 'i' + one input byte, 'r' + four bytes
 * (rows and columns, big-endian), or 'd'/'q' without payload.
 * Server-to-client stream: raw PTY output.
 */
#define MSG_INPUT 'i'
#define MSG_RESIZE 'r'
#define MSG_DETACH 'd'
#define MSG_QUIT 'q'

int session_socket_path(char *buffer, size_t capacity);
