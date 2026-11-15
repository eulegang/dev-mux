#include "runtime.h"

#include "../protocol/protocol.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <uv.h>

uv_connect_t client_conn_connect;

buffer *conn_out;
buffer *conn_in;
uv_write_t client_conn_write;
uv_write_t client_conn_rwrite;
uv_write_t client_conn_twrite;
uv_pipe_t client_conn;
const char *git_path;

uv_tty_t term_in;
uv_tty_t term_out;
uv_write_t term_write_phase;

char TERM_READ_BUF[1024];

static void client_term_read_alloc_cb(uv_handle_t *, size_t suggested,
                                      uv_buf_t *buf) {

  buf->base = TERM_READ_BUF;
  buf->len = suggested < 1024 ? suggested : 1024;
}

static void client_alloc_buffer(uv_handle_t *handle, size_t suggested,
                                uv_buf_t *buf) {
  (void)handle;
  buf->base = malloc(suggested);
  buf->len = buf->base ? suggested : 0;
}

static void client_conn_on_close(uv_handle_t *) {
  buffer_deinit(conn_in);
  buffer_deinit(conn_out);
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
  while (msg) {
    // syslog(LOG_INFO, "recieved message: %#02x", msg->tag);
    // server_handle_process(stream->handle, msg, conn_out);

    buffer_chomp(conn_in);
    msg = buffer_message(conn_in);
  }

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

static void client_term_on_read(uv_stream_t *stream, ssize_t nread,
                                const uv_buf_t *buf) {

  if (nread > 0) {
    buffer_push_msg(conn_out, TAG_SESSION_SENDKEYS);
    buffer_push_data(conn_out, buf->base, (size_t)nread);
  }

  msg_tlv *msg = buffer_message(conn_out);
  if (msg) {
    uv_buf_t bufs[] = {
        {.base = (char *)msg, .len = sizeof(msg_tlv) + msg->len}};

    uv_write(&client_conn_twrite, (uv_stream_t *)&client_conn, bufs, 1,
             client_conn_on_write);
  }

  if (nread < 0) {
    if (nread == UV_EOF) {
      syslog(LOG_INFO, "terminal disconnected");
    } else {
      syslog(LOG_ERR, "termina read error: %s", uv_strerror((int)nread));
    }

    if (!uv_is_closing((uv_handle_t *)stream))
      uv_close((uv_handle_t *)stream, client_conn_on_close);
  }

  uv_read_start((uv_stream_t *)&term_in, client_term_read_alloc_cb,
                client_term_on_read);
}

static void client_on_connect(uv_connect_t *conn, int status) {
  const char *path = (const char *)conn->data;

  if (status < 0) {
    syslog(LOG_ERR, "failed to connect to %s", path);
    exit(EXIT_FAILURE);
  }

  syslog(LOG_INFO, "Pushing declare message");
  buffer_push_msg(conn_out, TAG_SESSION_JOIN);
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

static void client_term_mode_reset() {
  uv_tty_reset_mode();

  // may be hacky but on exit make one attempt to exit alternate screen
  static const char leave[] = "\x1b[?1049l";
  write(1, leave, sizeof(leave) - 1);
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

  if ((err = uv_tty_init(uv_default_loop(), &term_in, 0, 1))) {
    syslog(LOG_ERR, "failed to initialize term input");
    exit(EXIT_FAILURE);
  }

  if ((err = uv_tty_init(uv_default_loop(), &term_out, 1, 0))) {
    syslog(LOG_ERR, "failed to initialize term output");
    exit(EXIT_FAILURE);
  }

  atexit(client_term_mode_reset);

  if ((err = uv_tty_set_mode(&term_out, UV_TTY_MODE_RAW))) {
    syslog(LOG_ERR, "failed to initialize term output");
    exit(EXIT_FAILURE);
  }

  if ((err = uv_tty_set_mode(&term_in, UV_TTY_MODE_RAW))) {
    syslog(LOG_ERR, "failed to initialize term output");
    exit(EXIT_FAILURE);
  }

  client_conn_connect.data = path;
  uv_pipe_connect(&client_conn_connect, &client_conn, path, client_on_connect);

  uv_buf_t bufs[] = {
      {.base = "\x1b[?1049h\x1b[2J\x1b[H", .len = 5},
  };

  uv_write(&term_write_phase, (uv_stream_t *)&term_out, bufs, 1, NULL);

  uv_read_start((uv_stream_t *)&term_in, client_term_read_alloc_cb,
                client_term_on_read);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
