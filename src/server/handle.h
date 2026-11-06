#pragma once

#include "../protocol/protocol.h"

typedef struct {
  buffer *inbuf;
  buffer *outbuf;
  int fd;
  uint8_t session;
  uint8_t phase;
} server_handle;

server_handle *server_handle_init(int fd);
void server_handle_read(server_handle *);
void server_handle_process(server_handle *, msg_tlv *msg);
void server_handle_deinit(server_handle *);
