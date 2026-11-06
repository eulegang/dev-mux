#include "server.h"

#include "../protocol/protocol.h"

#include "runtime.h"

#include <limits.h>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

typedef enum {
  PHASE_INIT,
  PHASE_LOG,
  PHASE_LISTENING,
  PHASE_RUNTIME,
} phase_t;

static phase_t server_phase = PHASE_INIT;
static int server_socket = -1;
static char server_socket_path[PATH_MAX] = {0};

static void server_cleanup() {
  /// Meaning to fallthrough here
  switch (server_phase) {
  case PHASE_RUNTIME:
    server_runtime_deinit();
  case PHASE_LISTENING:
    if (server_socket != -1) {
      close(server_socket);
      unlink(server_socket_path);
    }
  case PHASE_LOG:
    closelog();
  case PHASE_INIT:
    break;
  }
}

static void server_connect(const char *buf) {
  struct sockaddr_un address = {.sun_family = AF_UNIX};
  if (buf != NULL) {
    if (strlcpy(server_socket_path, buf, PATH_MAX) >= PATH_MAX) {
      syslog(LOG_ERR, "failed to setup unix path");
      exit(EXIT_FAILURE);
    }
  } else {
    if (session_socket_path(server_socket_path, PATH_MAX) < 0) {
      syslog(LOG_ERR, "failed to setup unix path");
      exit(EXIT_FAILURE);
    }
  }

  if (strlcpy(address.sun_path, server_socket_path, sizeof(address.sun_path)) >=
      sizeof(address.sun_path)) {
    syslog(LOG_ERR, "failed to setup unix path");
    exit(EXIT_FAILURE);
  }

  server_socket = socket(AF_UNIX, SOCK_STREAM, 0);

  if (server_socket < 0) {
    syslog(LOG_ERR, "failed to open unix socket: %m");
    exit(EXIT_FAILURE);
  }

  mode_t old_mask = umask(0077);
  int res = bind(server_socket, (struct sockaddr *)&address, sizeof(address));
  umask(old_mask);
  if (res < 0) {
    syslog(LOG_ERR, "bind (session already running or stale socket?) %s: %m",
           server_socket_path);
    exit(EXIT_FAILURE);
  }

  if (listen(server_socket, 4) < 0) {
    syslog(LOG_ERR, "failed to listen for clients: %m");
    exit(EXIT_FAILURE);
  }
}

static volatile sig_atomic_t stopping;
static void request_stop(int signo) {
  (void)signo;
  stopping = 1;
}

static void server_setup_signals() {
  struct sigaction action = {0};
  action.sa_handler = request_stop;
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGINT, &action, NULL) < 0 ||
      sigaction(SIGTERM, &action, NULL) < 0 ||
      sigaction(SIGHUP, &action, NULL) < 0) {
    syslog(LOG_ERR, "unable to setup signal handlers: %m");
    exit(EXIT_FAILURE);
  }

  action.sa_handler = SIG_IGN;
  if (sigaction(SIGPIPE, &action, NULL) < 0) {
    syslog(LOG_ERR, "unable to setup signal handlers: %m");
    exit(EXIT_FAILURE);
  }
}

int run_server(server_opt_t options) {
  atexit(server_cleanup);

  openlog("dev-mux", LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_DAEMON);
  server_phase = PHASE_LOG;

  server_connect(options.unix);
  server_phase = PHASE_LISTENING;

  server_setup_signals();

  if (server_runtime_init(server_socket)) {
    exit(EXIT_FAILURE);
  }

  server_phase = PHASE_RUNTIME;

  syslog(LOG_INFO, "server started on %s", server_socket_path);

  server_runtime_run();

  return 0;
}
