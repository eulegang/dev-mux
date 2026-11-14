#include "runtime.h"

#include "../protocol/protocol.h"

#include <assert.h>
#include <stdlib.h>
#include <syslog.h>
#include <uv.h>

uv_connect_t client_conn_connect;

buffer *conn_out;
buffer *conn_in;
uv_write_t client_conn_write;
uv_pipe_t client_conn;

static void client_conn_on_write(uv_write_t *write, int status) {
  if (status < 0) {
    syslog(LOG_ERR, "failed to write out file");
    exit(EXIT_FAILURE);
  }

  buffer_chomp(conn_out);

  msg_tlv *msg = buffer_message(conn_out);
  if (msg) {
    uv_buf_t buf = buffer_uv(conn_out);
    uv_write(&client_conn_write, write->handle, &buf, 1, client_conn_on_write);
  }
}

static void client_on_connect(uv_connect_t *conn, int status) {
  const char *path = (const char *)conn->data;

  if (status < 0) {
    syslog(LOG_ERR, "failed to connect to %s", path);
    exit(EXIT_FAILURE);
  }

  buffer_push_msg(conn_out, TAG_SESSION_HELLO);

  uv_buf_t buf = buffer_uv(conn_out);

  uv_write(&client_conn_write, conn->handle, &buf, 1, client_conn_on_write);
}

void client_runtime_start(char *path) {
  int err;

  conn_out = buffer_init(4096);
  conn_in = buffer_init(4096);

  assert(conn_out);
  assert(conn_in);

  if ((err = uv_pipe_init(uv_default_loop(), &client_conn, 0))) {
    syslog(LOG_ERR, "failed to initialize uv socket");
    exit(EXIT_FAILURE);
  }

  client_conn.data = path;
  uv_pipe_connect(&client_conn_connect, &client_conn, path, client_on_connect);
}
