#pragma once

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <uv.h>

typedef struct msg_tlv {
  uint16_t tag;
  uint16_t len;
  uint8_t bytes[];
} msg_tlv;

#define TAG_SESSION_JOIN 0x90
#define TAG_SESSION_SENDKEYS 0x91

#define TAG_SESSION_ERROR 0xA0
#define TAG_SESSION_OK 0xA0

int session_socket_path(char *buffer, size_t capacity);

typedef struct buffer {
  uint16_t len;
  uint16_t cap;
  uint16_t last_msg;
  char *buffer;
} buffer;

buffer *buffer_init(uint16_t cap);
void buffer_deinit(buffer *);

ssize_t buffer_push_msg(buffer *, uint16_t tag);
ssize_t buffer_push_data(buffer *, const char *bytes, size_t len);
ssize_t buffer_push_raw(buffer *, const char *bytes, size_t len);
uv_buf_t buffer_uv(buffer *);

msg_tlv *buffer_message(buffer *);
void buffer_chomp(buffer *);
