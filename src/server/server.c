#include "server.h"

#include "runtime.h"

#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

int run_server(server_opt_t options) {
  openlog("dev-mux", LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_DAEMON);

  syslog(LOG_INFO, "server starting on %s", options.unix);
  server_runtime_start(options.unix);

  closelog();

  return 0;
}
