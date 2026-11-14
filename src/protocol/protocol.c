#include "protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int session_socket_path(char *buffer, size_t capacity) {
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  const char *home = getenv("HOME");
  int length;
  if (runtime != NULL && *runtime != '\0') {
    length = snprintf(buffer, capacity, "%s/dev-mux-%lu.sock", runtime,
                      (unsigned long)getuid());
  } else if (home != NULL && *home != '\0') {
    length = snprintf(buffer, capacity, "%s/.dev-mux.sock", home);
  } else {
    return -1;
  }
  return length >= 0 && (size_t)length < capacity ? 0 : -1;
}

buffer *buffer_init(uint16_t cap) {
  buffer *buffer = malloc(sizeof(struct buffer));
  if (!buffer)
    return NULL;

  buffer->buffer = malloc(cap);
  buffer->cap = cap;
  buffer->len = 0;
  buffer->last_msg = 0xffff;

  return buffer;
}

void buffer_deinit(buffer *self) {
  if (self) {
    free(self->buffer);
    free(self);
  }
}

msg_tlv *buffer_message(buffer *self) {
  if (self->len < sizeof(msg_tlv)) {
    return NULL;
  }

  msg_tlv *msg = (msg_tlv *)self->buffer;

  if (msg->len < self->len + sizeof(msg_tlv)) {
    return NULL;
  }

  return msg;
}

void buffer_chomp(buffer *self) {
  msg_tlv *msg = buffer_message(self);
  if (msg) {
    uint16_t len = msg->len;

    memcpy(self->buffer, self->buffer + len, len);
    self->len -= len;
  }
}

ssize_t buffer_push_msg(buffer *self, uint16_t tag) {
  if (self->len + sizeof(msg_tlv) >= self->cap) {
    return -1;
  }

  self->last_msg = self->len;

  msg_tlv *msg = (msg_tlv *)(self->buffer + self->len);

  msg->tag = tag;
  msg->len = 0;

  return 0;
}

ssize_t buffer_push_data(buffer *self, const char *bytes, size_t len) {
  if (self->last_msg == 0xFFFF) {
    return -1;
  }

  if (self->len + len >= self->cap) {
    return -1;
  }

  msg_tlv *msg = (msg_tlv *)(self->buffer + self->last_msg);

  memcpy(self->buffer + self->len, bytes, len);
  self->len += len;
  msg->len += (uint16_t)len;

  return 0;
}

ssize_t buffer_push_raw(buffer *self, const char *bytes, size_t len) {
  if (self->len + len >= self->cap) {
    return -1;
  }

  memcpy(self->buffer + self->len, bytes, len);
  self->len += len;

  return 0;
}

uv_buf_t buffer_uv(buffer *self) {
  return (uv_buf_t){.base = self->buffer, .len = self->len};
}
