#include "pty.h"

#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <util.h>
#include <uv.h>

#define DEFAULT_LINES 65
#define DEFAULT_COLS 202

struct server_pty {
  uv_pipe_t pipe;
  uv_process_t proc;
  int fd;
  char status;
};

static void server_pty_on_close_pipe(uv_handle_t *pipe) {
  server_pty *pty = pipe->data;

  pty->status |= 2;
  if (pty->status & 0x03) {
    free(pty);
  }
}

static void server_pty_on_close_proc(uv_handle_t *proc) {
  server_pty *pty = proc->data;

  pty->status |= 1;
  if (pty->status & 0x03) {
    free(pty);
  }
}

static void server_pty_on_exit(uv_process_t *req, int64_t, int) {
  server_pty *pty = req->data;

  uv_close((uv_handle_t *)&pty->pipe, server_pty_on_close_pipe);
  uv_close((uv_handle_t *)&pty->proc, server_pty_on_close_proc);
}

server_pty *server_pty_fork(server_pty_opts opts) {
  int mfd, sfd;
  struct winsize size;
  size.ws_col = DEFAULT_COLS;
  size.ws_row = DEFAULT_LINES;

  server_pty *self = malloc(sizeof(server_pty));

  if (openpty(&mfd, &sfd, NULL, NULL, &size) < 0) {
    goto err_free;
  }

  fcntl(mfd, F_SETFD, FD_CLOEXEC);
  self->fd = mfd;
  self->status = 0;

  if (uv_pipe_init(opts.loop, &self->pipe, 0) < 0) {
    goto err_close;
  }

  if (uv_pipe_open(&self->pipe, mfd) < 0) {
    goto err_close;
  }

  uv_process_options_t spawn_opts;

  char *args[3];
  args[0] = "zsh";
  args[1] = "-i";
  args[2] = NULL;

  uv_stdio_container_t stdio[3];
  for (int i = 0; i < 3; i++) {
    stdio[i].data.fd = sfd;
    stdio[i].flags = UV_INHERIT_FD;
  }

  spawn_opts.args = args;
  spawn_opts.cwd = opts.cwd;
  spawn_opts.file = "zsh";
  spawn_opts.flags = UV_PROCESS_DETACHED;
  spawn_opts.stdio = stdio;
  spawn_opts.stdio_count = 3;
  spawn_opts.exit_cb = server_pty_on_exit;

  if (uv_spawn(opts.loop, &self->proc, &spawn_opts) < 0) {
    goto err_close;
  }

  close(sfd);

  self->pipe.data = self;
  self->proc.data = self;

  return self;

err_close:
  close(mfd);
  close(sfd);

err_free:
  free(self);
  return NULL;
}

s32 server_pty_resize(server_pty *self, u16 cols, u16 lines) {
  struct winsize ws = {
      .ws_row = lines,
      .ws_col = cols,
  };

  return ioctl(self->fd, TIOCSWINSZ, &ws);
}

s32 server_pty_kill(server_pty *self, s32 signal) {
  return uv_process_kill(&self->proc, signal);
}

uv_stream_t *server_pty_stream(server_pty *self) { return (uv_stream_t *)self; }

void server_pty_deinit(server_pty *pty) {
  uv_close((uv_handle_t *)&pty->pipe, server_pty_on_close_pipe);
  uv_close((uv_handle_t *)&pty->proc, server_pty_on_close_proc);
}
