#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>

#include "signals.h"

static void request_stop(int signo) { (void)signo; }

void server_setup_signals() {
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
