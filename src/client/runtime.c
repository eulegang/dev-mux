#include "runtime.h"

#include "../protocol/protocol.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <uv.h>

uv_connect_t client_conn_connect;

buffer *conn_out;
buffer *conn_in;
uv_write_t client_conn_write;
uv_write_t client_conn_rwrite;
uv_pipe_t client_conn;
const char *git_path;

static void client_alloc_buffer(uv_handle_t *handle, size_t suggested,
                                uv_buf_t *buf) {
  (void)handle;
  buf->base = malloc(suggested);
  buf->len = buf->base ? suggested : 0;
}

static void client_conn_on_close(uv_handle_t *) {
  buffer_deinit(conn_in);
  buffer_deinit(conn_out);
  // TODO: figure out handle lifetime
  // free(conn);
  // free(handle);
}

static void client_conn_on_write(uv_write_t *write, int status) {
  if (status < 0) {
    syslog(LOG_ERR, "failed to write out file");
    exit(EXIT_FAILURE);
  }

  syslog(LOG_INFO, "written message");

  buffer_chomp(conn_out);

  msg_tlv *msg = buffer_message(conn_out);
  if (msg) {
    uv_buf_t buf = uv_buf_init((char *)msg, sizeof(*msg) + msg->len);
    uv_write(&client_conn_write, write->handle, &buf, 1, client_conn_on_write);
  }
}

static void client_conn_on_read(uv_stream_t *stream, ssize_t nread,
                                const uv_buf_t *buf) {
  if (nread > 0) {
    buffer_push_raw(conn_in, buf->base, (size_t)nread);
  }

  if (buf) {
    free(buf->base);
  }

  if (uv_is_closing((uv_handle_t *)stream))
    return;

  msg_tlv *msg = buffer_message(conn_in);
  // while (msg) {
  //   syslog(LOG_INFO, "recieved message: %#02x", msg->tag);
  //   server_handle_process(stream->handle, msg, conn_out);
  //   msg = buffer_message(conn->input);
  // }

  msg = buffer_message(conn_out);
  if (msg) {
    uv_buf_t bufs[] = {
        {.base = (char *)msg, .len = sizeof(msg_tlv) + msg->len}};

    uv_write(&client_conn_rwrite, stream, bufs, 1, client_conn_on_write);
  }

  if (nread < 0) {
    if (nread == UV_EOF) {
      syslog(LOG_INFO, "client disconnected");
    } else {
      syslog(LOG_ERR, "connection read error: %s", uv_strerror((int)nread));
    }

    if (!uv_is_closing((uv_handle_t *)stream))
      uv_close((uv_handle_t *)stream, client_conn_on_close);
  }
}

static void client_on_connect(uv_connect_t *conn, int status) {
  const char *path = (const char *)conn->data;

  if (status < 0) {
    syslog(LOG_ERR, "failed to connect to %s", path);
    exit(EXIT_FAILURE);
  }

  syslog(LOG_INFO, "Pushing declare message");
  buffer_push_msg(conn_out, TAG_SESSION_DECLARE);
  buffer_push_data(conn_out, git_path, strlen(git_path) + 1);

  msg_tlv *msg = buffer_message(conn_out);
  if (msg) {
    syslog(LOG_INFO, "writing message out");
    uv_buf_t buf = uv_buf_init((char *)msg, sizeof(*msg) + msg->len);
    uv_write(&client_conn_write, conn->handle, &buf, 1, client_conn_on_write);
  } else {
    syslog(LOG_ERR, "no message to send");
  }

  uv_read_start(conn->handle, client_alloc_buffer, client_conn_on_read);
}

void client_runtime_start(char *path, const char *repository_root) {
  git_path = repository_root;
  int err;

  conn_out = buffer_init(4096);
  conn_in = buffer_init(4096);

  assert(conn_out);
  assert(conn_in);

  if ((err = uv_pipe_init(uv_default_loop(), &client_conn, 0))) {
    syslog(LOG_ERR, "failed to initialize uv socket");
    exit(EXIT_FAILURE);
  }

  client_conn_connect.data = path;
  uv_pipe_connect(&client_conn_connect, &client_conn, path, client_on_connect);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
