#include "pty.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

typedef struct {
  uv_pipe_t master;
  uv_signal_t child_signal;
  pid_t child;
  unsigned int open_handles;
} server_pty;

static server_pty *active_pty;

static void pty_on_close(uv_handle_t *handle) {
  server_pty *pty = handle->data;
  if (--pty->open_handles == 0) {
    if (pty->child > 0) {
      kill(pty->child, SIGHUP);
      // Do not block the event loop waiting for the child to exit.
      waitpid(pty->child, NULL, WNOHANG);
    }
    if (active_pty == pty)
      active_pty = NULL;
    free(pty);
  }
}

static void pty_close(uv_handle_t *handle) {
  if (!uv_is_closing(handle))
    uv_close(handle, pty_on_close);
}

static void pty_alloc(uv_handle_t *handle, size_t suggested, uv_buf_t *buf) {
  (void)handle;
  buf->base = malloc(suggested);
  buf->len = buf->base ? suggested : 0;
}

static void pty_on_read(uv_stream_t *stream, ssize_t nread,
                        const uv_buf_t *buf) {
  free(buf->base);
  if (nread < 0) {
    if (nread != UV_EOF)
      syslog(LOG_ERR, "PTY read failed: %s", uv_strerror((int)nread));
    server_pty *pty = stream->data;
    pty_close((uv_handle_t *)stream);
    int status;
    pid_t result = waitpid(pty->child, &status, WNOHANG);
    if (result == pty->child || (result < 0 && errno == ECHILD)) {
      pty->child = -1;
      pty_close((uv_handle_t *)&pty->child_signal);
    }
  }
  // Shell output is not yet forwarded to clients.
}

static void pty_on_child(uv_signal_t *signal, int signo) {
  (void)signo;
  server_pty *pty = signal->data;
  int status;
  pid_t result = waitpid(pty->child, &status, WNOHANG);
  if (result == pty->child || (result < 0 && errno == ECHILD)) {
    pty->child = -1;
    pty_close((uv_handle_t *)&pty->master);
    pty_close((uv_handle_t *)&pty->child_signal);
  }
}

int server_pty_start(uv_loop_t *loop) {
  server_pty *pty = calloc(1, sizeof(*pty));
  if (!pty)
    return UV_ENOMEM;

  int fd;
  pid_t child = forkpty(&fd, NULL, NULL, NULL);
  if (child < 0) {
    int err = uv_translate_sys_error(errno);
    free(pty);
    return err;
  }
  if (child == 0) {
    const char *shell = getenv("SHELL");
    if (!shell || !*shell)
      shell = "/bin/sh";
    execl(shell, shell, (char *)NULL);
    _exit(127);
  }

  pty->child = child;
  int err = uv_pipe_init(loop, &pty->master, 0);
  if (err) {
    close(fd);
    kill(child, SIGHUP);
    waitpid(child, NULL, WNOHANG);
    free(pty);
    return err;
  }
  pty->master.data = pty;
  pty->open_handles++;

  err = uv_pipe_open(&pty->master, fd);
  if (err) {
    close(fd);
    goto fail;
  }

  err = uv_signal_init(loop, &pty->child_signal);
  if (err)
    goto fail;
  pty->child_signal.data = pty;
  pty->open_handles++;

  err = uv_signal_start(&pty->child_signal, pty_on_child, SIGCHLD);
  if (err)
    goto fail;

  err = uv_read_start((uv_stream_t *)&pty->master, pty_alloc, pty_on_read);
  if (err)
    goto fail;

  active_pty = pty;
  return 0;

fail:
  pty_close((uv_handle_t *)&pty->master);
  if (pty->open_handles == 2)
    pty_close((uv_handle_t *)&pty->child_signal);
  return err;
}

void server_pty_stop(void) {
  if (!active_pty)
    return;
  pty_close((uv_handle_t *)&active_pty->master);
  pty_close((uv_handle_t *)&active_pty->child_signal);
}
