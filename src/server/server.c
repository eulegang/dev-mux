#include "server.h"
#include "../protocol/protocol.h"

#include "runtime.h"
#include "signals.h"

#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

typedef enum {
  PHASE_INIT,
  PHASE_LOG,
  PHASE_LISTENING,
  PHASE_RUNTIME,
} phase_t;

static phase_t server_phase = PHASE_INIT;

static void server_cleanup() {
  /// Meaning to fallthrough here
  switch (server_phase) {
  case PHASE_RUNTIME:
    server_runtime_deinit();
  case PHASE_LISTENING:
    socket_deinit();
  case PHASE_LOG:
    closelog();
  case PHASE_INIT:
    break;
  }
}

char server_unix_path[PATH_MAX];

int run_server(server_opt_t options) {
  atexit(server_cleanup);

  openlog("dev-mux", LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_DAEMON);
  server_phase = PHASE_LOG;

  socket_serve(options.unix);
  server_phase = PHASE_LISTENING;

  server_setup_signals();

  if (server_runtime_init(socket_fd)) {
    exit(EXIT_FAILURE);
  }

  server_phase = PHASE_RUNTIME;

  syslog(LOG_INFO, "server started on %s", socket_path);

  server_runtime_run();

  return 0;
}
