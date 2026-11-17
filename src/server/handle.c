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

bool server_handle_process(server_handle *, msg_tlv *msg, buffer *out) {
  syslog(LOG_INFO, "Handling a message: %#02x", msg->tag);

  switch (msg->tag) {
  case TAG_SESSION_JOIN: {
    buffer_push_msg(out, TAG_SESSION_OK);
  } break;

  case TAG_SESSION_SENDKEYS: {
    for (size_t i = 0; i < msg->len; i++) {
      uint8_t ch = msg->bytes[i];
      if (ch >= ' ' && ch <= '~') {
        syslog(LOG_INFO, "key sent: %c", ch);
      } else {
        syslog(LOG_INFO, "key sent: %#02x", ch);
      }
    }
  } break;

  default:
    server_handle_send_err(out, "improper init message sent");
    syslog(LOG_ERR, "found unhandled tag %#02x", msg->tag);
    break;
  }

  return true;
}
