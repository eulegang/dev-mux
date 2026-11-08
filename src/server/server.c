#include "server.h"

#include "network.h"
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
