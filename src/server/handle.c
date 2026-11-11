#include "handle.h"

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define SESSION_UNINIT 0xFF
#define SESSION_INIT 0xFE

server_handle *server_handle_init(int fd) {
  server_handle *self = malloc(sizeof(server_handle));

  if (!self)
    return NULL;

  self->inbuf = buffer_init(0x1000);

  if (!self->inbuf) {
    free(self);
    return NULL;
  }

  self->outbuf = buffer_init(0x1000);
  if (!self->outbuf) {
    free(self->inbuf);
    free(self);
    return NULL;
  }

  self->fd = fd;
  self->session = 0xFF;

  return self;
}

void server_handle_read(server_handle *self) {
  ssize_t n = buffer_fill(self->inbuf, self->fd);
  syslog(LOG_DEBUG, "reading %zd bytes from client", n);
  if (n < 0) {
    syslog(LOG_ERR, "client connection buffer overflown");
    server_handle_deinit(self);
    return;
  }

  if (n == 0) {
    server_handle_deinit(self);
    return;
  }

  msg_tlv *msg = buffer_message(self->inbuf);

  if (msg) {
    syslog(LOG_DEBUG, "successfully found a message");
    server_handle_process(self, msg);

    buffer_chomp(self->inbuf);
  }
}

void server_handle_deinit(server_handle *self) {
  if (self) {
    buffer_deinit(self->inbuf);
    buffer_deinit(self->outbuf);

    close(self->fd);
  }

  free(self);
}

static ssize_t server_handle_send_err(server_handle *self, const char *msg) {
  ssize_t res = buffer_push_msg(self->outbuf, TAG_SESSION_ERROR);

  if (res < 0)
    return res;

  res = buffer_push_data(self->outbuf, (const uint8_t *)msg, strlen(msg) + 1);

  if (res < 0)
    return res;

  res = buffer_flush(self->outbuf, self->fd);

  return res;
}

void server_handle_process(server_handle *self, msg_tlv *msg) {
  if (self->session == SESSION_UNINIT) {
    if (msg->tag == TAG_SESSION_HELLO) {
      self->session = SESSION_INIT;

      buffer_push_msg(self->outbuf, TAG_SESSION_HELLO_RESPONSE);
      buffer_flush(self->outbuf, self->fd);
    } else {
      server_handle_send_err(self, "improper init message sent");
      server_handle_deinit(self);
    }

    return;
  }
}
