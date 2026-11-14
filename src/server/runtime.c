#include "runtime.h"

#include <assert.h>
#include <stdlib.h>
#include <syslog.h>
#include <uv.h>

#include "../protocol/protocol.h"
#include "handle.h"

typedef struct {
  buffer *input;
  buffer *output;
  server_handle *handle;
  uv_write_t *req;
} server_conn;

#define GET_CONN(s) ((server_conn *)(s)->data)

static void server_alloc_buffer(uv_handle_t *handle, size_t suggested,
                                uv_buf_t *buf) {
  (void)handle;
  buf->base = malloc(suggested);
  buf->len = buf->base ? suggested : 0;
}

static void server_conn_on_close(uv_handle_t *handle) {
  auto conn = GET_CONN(handle);

  buffer_deinit(conn->input);
  server_handle_deinit(conn->handle);
  free(conn);
  free(handle);
}

static void server_conn_on_write(uv_write_t *write, int status) {
  if (status < 0) {
    uv_close((uv_handle_t *)write->handle, server_conn_on_close);
  }

  auto conn = GET_CONN(write);

  auto handle = write->handle;

  msg_tlv *msg = buffer_message(conn->output);
  if (msg) {
    free(conn->req);
    conn->req = malloc(sizeof(uv_write_t));
    conn->req->data = conn;
    uv_buf_t bufs[] = {
        {.base = (char *)msg, .len = sizeof(msg_tlv) + msg->len}};

    uv_write(conn->req, handle, bufs, 1, server_conn_on_write);
  }
}

static void server_conn_on_read(uv_stream_t *stream, ssize_t nread,
                                const uv_buf_t *buf) {
  server_conn *conn = (server_conn *)stream->data;
  if (nread > 0) {
    buffer_push_raw(conn->input, (const uint8_t *)buf->base, buf->len);
  }

  if (buf) {
    free(buf->base);
  }

  msg_tlv *msg = buffer_message(conn->input);
  while (msg) {
    server_handle_process(conn->handle, msg, conn->output);
    msg = buffer_message(conn->input);
  }

  msg = buffer_message(conn->output);
  if (msg) {
    conn->req = malloc(sizeof(uv_write_t));
    conn->req->data = conn;
    uv_buf_t bufs[] = {
        {.base = (char *)msg, .len = sizeof(msg_tlv) + msg->len}};

    uv_write(conn->req, stream, bufs, 1, server_conn_on_write);
  }

  if (nread < 0) {
    if (nread == UV_EOF) {
      syslog(LOG_INFO, "client disconnected");
    } else {
      syslog(LOG_INFO, "connection read error: %s", uv_strerror((int)nread));
    }

    uv_close((uv_handle_t *)stream, server_conn_on_close);
  }
}

// TODO: handle failures in this function
static void server_connect(uv_stream_t *server, int status) {
  if (status < 0)
    return;

  uv_pipe_t *peer = malloc(sizeof(*peer));
  if (!peer)
    return;

  buffer *buffer = buffer_init(4096);
  server_handle *handle = server_handle_init();

  server_conn *conn = malloc(sizeof(server_conn));

  assert(buffer);
  assert(handle);
  assert(conn);

  conn->input = buffer;
  conn->handle = handle;
  peer->data = conn;

  uv_pipe_init(server->loop, peer, 0);

  if (uv_accept(server, (uv_stream_t *)peer) == 0) {
    uv_read_start((uv_stream_t *)peer, server_alloc_buffer,
                  server_conn_on_read);
  } else {
    uv_close((uv_handle_t *)peer, server_conn_on_close);
  }
}

void server_runtime_start(char *path) {
  uv_pipe_t server;
  int err;

  if ((err = uv_pipe_init(uv_default_loop(), &server, 0))) {
    syslog(LOG_ERR, "failed to initialize socket: %s", uv_strerror(err));
    exit(EXIT_FAILURE);
  }

  err = uv_pipe_bind(&server, path);
  if (err) {
    syslog(LOG_ERR, "failed to bind unix path: %s", uv_strerror(err));
    exit(EXIT_FAILURE);
  }

  err = uv_listen((uv_stream_t *)&server, 16, server_connect);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
