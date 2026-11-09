#include "signals.h"
#include "stdlib.h"

#include <syslog.h>

volatile sig_atomic_t resize_pending;
volatile sig_atomic_t stop_requested;

static void on_signal(int signo) {
  if (signo == SIGWINCH) {
    resize_pending = 1;
  } else {
    stop_requested = 1;
  }
}

static int install_handler(int signo) {
  struct sigaction action = {0};
  action.sa_handler = on_signal;
  sigemptyset(&action.sa_mask);
  return sigaction(signo, &action, NULL);
}

void install_signals(void) {
  struct sigaction ignore = {0};
  ignore.sa_handler = SIG_IGN;
  sigemptyset(&ignore.sa_mask);
  if (install_handler(SIGWINCH) < 0 || install_handler(SIGINT) < 0 ||
      install_handler(SIGTERM) < 0 || install_handler(SIGHUP) < 0 ||
      sigaction(SIGPIPE, &ignore, NULL) < 0) {
    syslog(LOG_ERR, "failed to install signal handlers %m");

    exit(EXIT_FAILURE);
  }
}
