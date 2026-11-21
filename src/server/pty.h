#pragma once

#include <uv.h>

typedef struct server_pty server_pty;

typedef struct server_pty_opts {
  uv_loop_t *loop;
  char *cwd;
} server_pty_opts;

server_pty *server_pty_fork(server_pty_opts opts);
s32 server_pty_resize(server_pty *, u16 cols, u16 lines);
void server_pty_deinit(server_pty *);
