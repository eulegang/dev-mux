#include "client.h"

#include "../protocol/protocol.h"
#include "terminal.h"

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>

#include "../reactor.h"

typedef enum {
  PHASE_INIT,
  PHASE_LOG,
  PHASE_SOCKET,
  PHASE_TERMINAL,
} PHASE;

static PHASE client_phase = PHASE_INIT;

static void client_cleanup(void) {
  switch (client_phase) {
  case PHASE_TERMINAL:
    terminal_exit();

  case PHASE_SOCKET:
    socket_deinit();

  case PHASE_LOG:
    closelog();

  case PHASE_INIT:
    break;
  }
}

char client_unix_path[PATH_MAX];
int run_client(client_opt_t options) {
  atexit(client_cleanup);

  openlog("dev-mux-cli", LOG_PID | LOG_NDELAY, LOG_USER);
  client_phase = PHASE_LOG;

  assert_termial();
  socket_connect(options.unix);
  client_phase = PHASE_SOCKET;
  terminal_init();
  terminal_enter();
  client_phase = PHASE_TERMINAL;
  int result = EXIT_FAILURE;

  reactor_t *reactor = reactor_init();

  // reactor_add_read(reactor, 0, NULL, NULL);
  // reactor_add_read(reactor, socket_fd, NULL, NULL);
  //
  // reactor_add_write(reactor, 1, NULL, NULL);
  // reactor_add_write(reactor, socket_fd, NULL, NULL);

  return result;
}
