#include "handle.h"

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define SESSION_UNINIT 0xFF
#define SESSION_INIT 0xFE

struct server_handle {
  uint16_t session;
  uint16_t phase;
};

server_handle *server_handle_init() {
  server_handle *self = malloc(sizeof(server_handle));

  if (!self)
    return NULL;

  self->session = 0xFF;

  return self;
}

void server_handle_deinit(server_handle *self) { free(self); }

static ssize_t server_handle_send_err(buffer *out, const char *msg) {
  ssize_t res = buffer_push_msg(out, TAG_SESSION_ERROR);

  if (res < 0)
    return res;

  res = buffer_push_data(out, msg, strlen(msg) + 1);

  if (res < 0)
    return res;

  return 0;
}

bool server_handle_process(server_handle *self, msg_tlv *msg, buffer *out) {
  syslog(LOG_INFO, "Handling a message: %d", msg->tag);

  if (self->session == SESSION_UNINIT) {
    if (msg->tag == TAG_SESSION_HELLO) {
      self->session = SESSION_INIT;

      buffer_push_msg(out, TAG_SESSION_HELLO_RESPONSE);
    } else {
      server_handle_send_err(out, "improper init message sent");
      return false;
    }
  }
  return true;
}
