#include "server.h"

#include "runtime.h"

#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

int run_server(server_opt_t options) {
  openlog("dev-mux", LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_DAEMON);

  if (options.force) {
    syslog(LOG_INFO, "removing previous socket");

    if (!unlink(options.unix)) {
      syslog(LOG_INFO, "successfully removed socket");
    }
  } else {
    syslog(LOG_INFO, "not forcing it");
  }

  syslog(LOG_INFO, "server starting on %s", options.unix);
  int status = server_runtime_start(options.unix);

  closelog();

  return status;
}
