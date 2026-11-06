#pragma once

#include <stdint.h>
#include <sys/types.h>

/* Client-to-server stream: 'i' + one input byte, 'r' + four bytes
 * (rows and columns, big-endian), or 'd'/'q' without payload.
 * Server-to-client stream: raw PTY output.
 */
#define MSG_INPUT 'i'
#define MSG_RESIZE 'r'
#define MSG_DETACH 'd'
#define MSG_QUIT 'q'

typedef struct msg_tlv {
  uint16_t tag;
  uint16_t len;
  uint8_t bytes[];
} msg_tlv;

#define TAG_SESSION_HELLO 0x90
#define TAG_SESSION_HELLO_RESPONSE 0x50
#define TAG_SESSION_ERROR 0xA0
#define TAG_SESSION_CREATE 0x80
#define TAG_SESSION_CREATE_RESPONSE 0x40

int session_socket_path(char *buffer, size_t capacity);

int write_all(int fd, const void *data, size_t size);

typedef struct buffer {
  uint16_t len;
  uint16_t cap;
  uint16_t last_msg;
  char *buffer;
} buffer;

buffer *buffer_init(uint16_t cap);
void buffer_deinit(buffer *);

ssize_t buffer_push_msg(buffer *, uint16_t tag);
ssize_t buffer_push_data(buffer *, const uint8_t *bytes, size_t len);
ssize_t buffer_flush(buffer *, int fd);

ssize_t buffer_fill(buffer *, int);
msg_tlv *buffer_message(buffer *);
void buffer_chomp(buffer *);
